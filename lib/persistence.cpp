#include "lib/persistence.hpp"
#include "lib/dev_handler.hpp"
#include "lib/iso_burner.hpp"
#include "lib/mbr_gpt.hpp"
#include "lib/fs_creator.hpp"
#include "lib/bootloader.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <cmath>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>

namespace Persistence {
    
    bool createPersistencePartition(
        const std::string& device,
        size_t sizeInMB,
        FilesystemSupport::FSType fsType
    ) {
        Logs::info("Creating persistence partition of " + std::to_string(sizeInMB) + " MB");
        
        size_t deviceSize = DeviceHandler::getDeviceSize(device) / (1024 * 1024);
        
        if (sizeInMB > deviceSize * 0.8) {
            throw FilesystemError("Persistence size too large for device");
        }
        
        std::string persistDevice = device;
        if (device.back() >= '0' && device.back() <= '9') {
            persistDevice = device + "2";
        } else {
            persistDevice = device + "2";
        }
        
        Logs::info("Creating filesystem on " + persistDevice);
        std::string label = (fsType == FilesystemSupport::FSType::EXT4) ? 
                           "casper-rw" : "PERSISTENCE";
        
        FilesystemSupport::formatPartition(persistDevice, fsType, label);
        
        DeviceHandler::syncDevice(device);
        
        Logs::success("Persistence partition created: " + persistDevice);
        return true;
    }
    
    bool setupPersistence(
        const std::string& isoPath,
        const std::string& device,
        size_t persistenceSizeMB,
        FilesystemSupport::FSType fsType,
        BootStructures::TableType tableType
    ) {
        Logs::info("Setting up bootable USB with persistence");
        
        std::string tableTypeStr = (tableType == BootStructures::TableType::MBR) ? "MBR" : "GPT";
        Logs::info("Using " + tableTypeStr + " partition table");
        
        size_t isoSize = ISOBurner::getISOSize(isoPath);
        size_t deviceSize = DeviceHandler::getDeviceSize(device);
        
        size_t isoSizeMB = (isoSize / (1024 * 1024)) + 200;
        size_t deviceSizeMB = deviceSize / (1024 * 1024);
        
        Logs::info("Device capacity: " + std::to_string(deviceSizeMB) + " MB");
        Logs::info("ISO size: " + std::to_string(isoSizeMB) + " MB");
        Logs::info("Requested persistence: " + std::to_string(persistenceSizeMB) + " MB");
        
        // Calculate required space (ISO + persistence + overhead)
        size_t overheadMB = 100; // MBR/GPT, alignment, filesystem overhead
        size_t totalRequired = isoSizeMB + persistenceSizeMB + overheadMB;
        
        Logs::info("Total required space: " + std::to_string(totalRequired) + " MB");
        
        // Check if device has enough space
        if (totalRequired > deviceSizeMB) {
            size_t availableForPersistence = deviceSizeMB - isoSizeMB - overheadMB;
            
            std::string errorMsg = "Insufficient storage space on device\n";
            errorMsg += "  Device capacity: " + std::to_string(deviceSizeMB) + " MB\n";
            errorMsg += "  ISO size: " + std::to_string(isoSizeMB) + " MB\n";
            errorMsg += "  Requested persistence: " + std::to_string(persistenceSizeMB) + " MB\n";
            errorMsg += "  Required total: " + std::to_string(totalRequired) + " MB\n";
            errorMsg += "  Shortage: " + std::to_string(totalRequired - deviceSizeMB) + " MB\n";
            
            if (availableForPersistence > 512) {
                errorMsg += "\nMaximum persistence you can use: " + 
                           std::to_string(availableForPersistence) + " MB";
            } else {
                errorMsg += "\nDevice is too small for persistence (minimum 512 MB required)";
            }
            
            throw FilesystemError(errorMsg);
        }
        
        // Calculate actual free space after ISO
        size_t freeSpaceAfterISO = deviceSizeMB - isoSizeMB - overheadMB;
        
        if (persistenceSizeMB < 512) {
            Logs::warning("Persistence size too small, setting to minimum 512 MB");
            persistenceSizeMB = 512;
            
            // Re-check after adjusting to minimum
            if (persistenceSizeMB > freeSpaceAfterISO) {
                throw FilesystemError("Insufficient storage: Device too small for minimum 512 MB persistence");
            }
        }
        
        // Warn if using more than 90% of available space
        if (persistenceSizeMB > (freeSpaceAfterISO * 0.9)) {
            Logs::warning("Persistence partition will use " + 
                         std::to_string((persistenceSizeMB * 100) / freeSpaceAfterISO) + 
                         "% of available space");
        }
        
        Logs::success("Storage check passed - sufficient space available");
        
        DeviceHandler::unmountDevice(device);
        
        Logs::info("Wiping existing data on device");
        DeviceHandler::wipeDevice(device);
        
        Logs::info("Creating " + tableTypeStr + " partition table");
        BootStructures::PartitionTable ptable(device, tableType);
        ptable.initialize();
        
        if (tableType == BootStructures::TableType::MBR) {
            ptable.createMBR();
        } else {
            ptable.createGPT();
        }
        
        uint32_t isoSectors = (isoSizeMB * 1024 * 1024) / 512;
        uint32_t persistSectors = (persistenceSizeMB * 1024 * 1024) / 512;
        uint32_t startSector = 2048;
        
        Logs::info("Creating ISO partition (" + std::to_string(isoSizeMB) + " MB)");
        
        try {
            ptable.addMBRPartition(startSector, isoSectors, 
                                   BootStructures::PartitionType::FAT32_LBA, true);
        } catch (const std::exception& e) {
            Logs::error("Failed to add ISO partition to MBR: " + std::string(e.what()));
            throw DeviceError(device, "Cannot create ISO partition in partition table");
        }
        
        Logs::info("Creating persistence partition (" + 
                  std::to_string(persistenceSizeMB) + " MB)");
        BootStructures::PartitionType persistType = 
            (fsType == FilesystemSupport::FSType::EXT4) ? 
            BootStructures::PartitionType::LINUX_NATIVE : 
            BootStructures::PartitionType::FAT32_LBA;
        
        try {
            ptable.addMBRPartition(startSector + isoSectors, persistSectors, persistType, false);
        } catch (const std::exception& e) {
            Logs::error("Failed to add persistence partition to MBR: " + std::string(e.what()));
            throw DeviceError(device, "Cannot create persistence partition in partition table");
        }
        
        ptable.commit();
        
        sleep(2);
        
        int fd = open(device.c_str(), O_RDONLY);
        if (fd >= 0) {
            ioctl(fd, BLKRRPART);
            close(fd);
        }
        
        system(("partprobe " + device + " 2>/dev/null").c_str());
        sleep(3);
        
        // Determine partition device names
        std::string part1, part2;
        if (device.find("nvme") != std::string::npos || 
            device.find("mmcblk") != std::string::npos) {
            part1 = device + "p1";
            part2 = device + "p2";
        } else {
            part1 = device + "1";
            part2 = device + "2";
        }
        
        // Verify partitions exist
        struct stat st;
        if (stat(part1.c_str(), &st) != 0) {
            Logs::warning("Partition " + part1 + " not found, waiting...");
            sleep(2);
            system(("partprobe " + device + " 2>/dev/null").c_str());
            sleep(2);
            
            if (stat(part1.c_str(), &st) != 0) {
                throw DeviceError(device, "Partition " + part1 + " was not created by kernel");
            }
        }
        
        Logs::success("Partitions created and verified: " + part1 + ", " + part2);
        
        Logs::info("Formatting first partition as FAT32");
        FilesystemCreator::createFilesystem(part1, "fat32", "MYISO");
        
        Logs::info("Burning ISO to first partition");
        ISOBurner::burnISO(isoPath, part1, ISOBurner::BurnMode::RAW);
        
        createPersistencePartition(device, persistenceSizeMB, fsType);
        
        Logs::info("Installing bootloader");
        Bootloader::installBootloader(device, isoPath);
        
        DeviceHandler::syncDevice(device);
        
        Logs::success("Bootable USB with persistence created successfully");
        return true;
    }
    
    size_t calculateOptimalSize(size_t isoSize, size_t deviceSize) {
        size_t isoSizeMB = isoSize / (1024 * 1024);
        size_t deviceSizeMB = deviceSize / (1024 * 1024);
        
        size_t availableSpace = deviceSizeMB - isoSizeMB - 200;
        
        if (availableSpace < 512) return 0;
        
        return std::min(availableSpace, static_cast<size_t>(16384));
    }
}
