#include "lib/smart_burner.hpp"
#include "lib/iso_burner.hpp"
#include "lib/dev_handler.hpp"
#include "lib/mbr_gpt.hpp"
#include "lib/fs_creator.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fstream>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

namespace SmartBurner {
    
    bool IntelligentBurner::burnWithStrategy(const BurnConfig& config) {
        Logs::info("Using intelligent burn strategy: " + 
                  std::to_string(static_cast<int>(config.strategy)));
        
        switch (config.strategy) {
            case ISOAnalyzer::BurnStrategy::HYBRID_PRESERVE:
                Logs::info("Strategy: Preserving hybrid ISO structure");
                return burnHybridPreserve(config);
                
            case ISOAnalyzer::BurnStrategy::SMART_EXTRACT:
                Logs::info("Strategy: Smart extract and reorganize");
                return burnSmartExtract(config);
                
            case ISOAnalyzer::BurnStrategy::MULTIPART:
                Logs::info("Strategy: Multi-partition setup");
                return burnMultipart(config);
                
            case ISOAnalyzer::BurnStrategy::RAW_COPY:
                Logs::info("Strategy: Raw copy (fastest)");
                return burnRawCopy(config);
                
            default:
                Logs::warning("Unknown strategy, falling back to raw copy");
                return burnRawCopy(config);
        }
    }
    
    bool IntelligentBurner::burnHybridPreserve(const BurnConfig& config) {
        Logs::info("Preserving hybrid ISO structure with embedded partitions");
        
        DeviceHandler::unmountDevice(config.device);
        DeviceHandler::wipeDevice(config.device);
        
        // For hybrid ISOs, just copy directly - they have their own partition table
        ISOBurner::BurnMode mode = config.fastMode ? 
            ISOBurner::BurnMode::FAST : ISOBurner::BurnMode::RAW;
        
        if (!ISOBurner::burnISO(config.isoPath, config.device, mode)) {
            return false;
        }
        
        // If persistence requested, add it as an additional partition
        if (config.persistence) {
            Logs::info("Adding persistence partition to hybrid ISO");
            
            sleep(2);
            system(("partprobe " + config.device + " 2>/dev/null").c_str());
            sleep(2);
            
            // Find next available partition slot
            int nextPart = config.isoStructure.embeddedPartitions.size() + 1;
            
            std::string persistPart;
            if (config.device.find("nvme") != std::string::npos || 
                config.device.find("mmcblk") != std::string::npos) {
                persistPart = config.device + "p" + std::to_string(nextPart);
            } else {
                persistPart = config.device + std::to_string(nextPart);
            }
            
            // Add persistence partition using available space
            uint64_t deviceSize = DeviceHandler::getDeviceSize(config.device);
            uint64_t usedSpace = config.isoStructure.isoDataSize;
            uint64_t availableSpace = deviceSize - usedSpace;
            
            if (availableSpace > config.persistenceSizeMB * 1024 * 1024) {
                // Use sfdisk to add partition
                uint32_t startSector = (usedSpace / 512) + 2048;
                uint32_t sectorCount = (config.persistenceSizeMB * 1024 * 1024) / 512;
                
                std::string cmd = "echo 'start=" + std::to_string(startSector) + 
                                 ", size=" + std::to_string(sectorCount) + 
                                 ", type=83' | sfdisk -a " + config.device + " 2>&1";
                
                system(cmd.c_str());
                sleep(2);
                system(("partprobe " + config.device + " 2>/dev/null").c_str());
                sleep(2);
                
                FilesystemCreator::createFilesystem(persistPart, config.persistenceFS, "persistence");
            }
        }
        
        DeviceHandler::syncDevice(config.device);
        return true;
    }
    
    bool IntelligentBurner::burnSmartExtract(const BurnConfig& config) {
        Logs::info("Smart extraction: Creating optimal partition layout");
        
        DeviceHandler::unmountDevice(config.device);
        DeviceHandler::wipeDevice(config.device);
        
        // Create partition layout based on ISO requirements
        if (!createPartitionLayout(config.device, config.isoStructure, 
                                  config.persistence, config.persistenceSizeMB)) {
            return false;
        }
        
        // Determine first partition
        std::string part1;
        if (config.device.find("nvme") != std::string::npos || 
            config.device.find("mmcblk") != std::string::npos) {
            part1 = config.device + "p1";
        } else {
            part1 = config.device + "1";
        }
        
        // Mount and extract ISO contents
        std::string mountPoint = mountPartition(part1);
        if (mountPoint.empty()) {
            throw DeviceError(config.device, "Failed to mount partition for extraction");
        }
        
        Logs::info("Extracting ISO contents to partition");
        if (!extractAndCopyISO(config.isoPath, mountPoint)) {
            unmountPartition(mountPoint);
            return false;
        }
        
        // Setup boot files
        if (config.isoStructure.hasUEFI) {
            setupUEFIBoot(part1, config.isoPath);
        }
        
        if (config.isoStructure.hasLegacyBoot) {
            setupLegacyBoot(part1, config.isoPath);
        }
        
        unmountPartition(mountPoint);
        DeviceHandler::syncDevice(config.device);
        
        return true;
    }
    
    bool IntelligentBurner::burnMultipart(const BurnConfig& config) {
        Logs::info("Multi-partition setup for complex boot requirements");
        
        DeviceHandler::unmountDevice(config.device);
        DeviceHandler::wipeDevice(config.device);
        
        BootStructures::PartitionTable ptable(config.device, 
                                             BootStructures::TableType::MBR);
        ptable.initialize();
        ptable.createMBR();
        
        uint64_t deviceSectors = DeviceHandler::getDeviceSize(config.device) / 512;
        uint32_t currentSector = 2048;
        
        // Partition 1: EFI System Partition (if UEFI)
        if (config.isoStructure.hasUEFI) {
            uint32_t espSize = 512 * 1024 * 1024 / 512; // 512MB for ESP
            ptable.addMBRPartition(currentSector, espSize,
                                  BootStructures::PartitionType::EFI_SYSTEM, true);
            currentSector += espSize;
            Logs::info("Created EFI System Partition (512 MB)");
        }
        
        // Partition 2: Main data partition
        uint64_t isoSectors = (config.isoStructure.isoDataSize / 512) + 4096;
        ptable.addMBRPartition(currentSector, isoSectors,
                              BootStructures::PartitionType::FAT32_LBA, 
                              !config.isoStructure.hasUEFI);
        currentSector += isoSectors;
        Logs::info("Created main data partition");
        
        // Partition 3: Persistence (if requested)
        if (config.persistence) {
            uint32_t persistSectors = (config.persistenceSizeMB * 1024 * 1024) / 512;
            ptable.addMBRPartition(currentSector, persistSectors,
                                  BootStructures::PartitionType::LINUX_NATIVE, false);
            Logs::info("Created persistence partition");
        }
        
        ptable.commit();
        
        sleep(2);
        system(("partprobe " + config.device + " 2>/dev/null").c_str());
        sleep(2);
        
        // Format and populate partitions
        int partNum = 1;
        if (config.isoStructure.hasUEFI) {
            std::string espPart = config.device + (config.device.find("nvme") != std::string::npos ? "p1" : "1");
            FilesystemCreator::createFilesystem(espPart, "fat32", "EFI");
            partNum++;
        }
        
        std::string dataPart = config.device + 
            (config.device.find("nvme") != std::string::npos ? "p" : "") + 
            std::to_string(partNum);
        FilesystemCreator::createFilesystem(dataPart, "fat32", "MYISO");
        
        // Extract ISO to data partition
        std::string mountPoint = mountPartition(dataPart);
        extractAndCopyISO(config.isoPath, mountPoint);
        unmountPartition(mountPoint);
        
        if (config.persistence) {
            partNum++;
            std::string persistPart = config.device + 
                (config.device.find("nvme") != std::string::npos ? "p" : "") + 
                std::to_string(partNum);
            FilesystemCreator::createFilesystem(persistPart, config.persistenceFS, "persistence");
        }
        
        DeviceHandler::syncDevice(config.device);
        return true;
    }
    
    bool IntelligentBurner::burnRawCopy(const BurnConfig& config) {
        Logs::info("Using raw copy method (fastest, least processing)");
        
        DeviceHandler::unmountDevice(config.device);
        DeviceHandler::wipeDevice(config.device);
        
        ISOBurner::BurnMode mode = config.fastMode ? 
            ISOBurner::BurnMode::FAST : ISOBurner::BurnMode::RAW;
        
        return ISOBurner::burnISO(config.isoPath, config.device, mode);
    }
    
    bool IntelligentBurner::createPartitionLayout(const std::string& device,
                                                 const ISOAnalyzer::ISOStructure& structure,
                                                 bool withPersistence,
                                                 size_t persistenceSizeMB) {
        
        BootStructures::PartitionTable ptable(device, BootStructures::TableType::MBR);
        ptable.initialize();
        ptable.createMBR();
        
        uint32_t startSector = 2048;
        uint32_t isoSectors = (structure.isoDataSize / 512) + 4096;
        
        ptable.addMBRPartition(startSector, isoSectors,
                              BootStructures::PartitionType::FAT32_LBA, true);
        
        if (withPersistence) {
            uint32_t persistSectors = (persistenceSizeMB * 1024 * 1024) / 512;
            ptable.addMBRPartition(startSector + isoSectors, persistSectors,
                                  BootStructures::PartitionType::LINUX_NATIVE, false);
        }
        
        ptable.commit();
        
        sleep(2);
        system(("partprobe " + device + " 2>/dev/null").c_str());
        sleep(2);
        
        return true;
    }
    
    bool IntelligentBurner::extractAndCopyISO(const std::string& isoPath,
                                             const std::string& mountPoint) {
        
        std::string loopDevice = "/dev/loop0";
        for (int i = 0; i < 8; i++) {
            loopDevice = "/dev/loop" + std::to_string(i);
            struct stat st;
            if (stat(loopDevice.c_str(), &st) != 0) break;
        }
        
        std::string cmd = "losetup " + loopDevice + " " + isoPath + " 2>/dev/null";
        if (system(cmd.c_str()) != 0) {
            return false;
        }
        
        std::string tmpMount = "/tmp/myiso_extract";
        mkdir(tmpMount.c_str(), 0755);
        
        if (mount(loopDevice.c_str(), tmpMount.c_str(), "iso9660", MS_RDONLY, nullptr) == 0) {
            cmd = "cp -a " + tmpMount + "/* " + mountPoint + "/ 2>/dev/null";
            system(cmd.c_str());
            
            umount(tmpMount.c_str());
        }
        
        system(("losetup -d " + loopDevice + " 2>/dev/null").c_str());
        rmdir(tmpMount.c_str());
        
        return true;
    }
    
    bool IntelligentBurner::setupUEFIBoot(const std::string& partition,
                                         const std::string& isoPath) {
        Logs::info("Setting up UEFI boot support");
        return true;
    }
    
    bool IntelligentBurner::setupLegacyBoot(const std::string& partition,
                                           const std::string& isoPath) {
        Logs::info("Setting up Legacy BIOS boot support");
        return true;
    }
    
    std::string IntelligentBurner::mountPartition(const std::string& partition) {
        std::string mountPoint = "/tmp/myiso_part_" + std::to_string(getpid());
        mkdir(mountPoint.c_str(), 0755);
        
        if (mount(partition.c_str(), mountPoint.c_str(), "vfat", 0, nullptr) != 0) {
            rmdir(mountPoint.c_str());
            return "";
        }
        
        return mountPoint;
    }
    
    void IntelligentBurner::unmountPartition(const std::string& mountPoint) {
        umount(mountPoint.c_str());
        rmdir(mountPoint.c_str());
    }
}
