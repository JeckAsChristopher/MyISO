#include "lib/iso_analyzer.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace ISOAnalyzer {
    
    ISOStructure SmartAnalyzer::analyzeISO(const std::string& isoPath) {
        Logs::info("Performing deep analysis of ISO structure...");
        
        ISOStructure structure;
        structure.isHybrid = checkHybridISO(isoPath);
        structure.hasElTorito = checkElTorito(isoPath);
        structure.hasUEFI = checkUEFI(isoPath);
        structure.hasLegacyBoot = structure.hasElTorito || structure.isHybrid;
        structure.embeddedPartitions = extractEmbeddedPartitions(isoPath);
        structure.bootFiles = findBootFiles(isoPath);
        
        // Determine if multi-boot
        structure.isMultiBoot = structure.hasUEFI && structure.hasLegacyBoot;
        
        // Calculate ISO data size
        std::ifstream file(isoPath, std::ios::binary | std::ios::ate);
        structure.isoDataSize = file.tellg();
        file.close();
        
        // Determine boot type
        if (structure.isMultiBoot) {
            structure.bootType = "Multi-Boot (UEFI + Legacy)";
        } else if (structure.hasUEFI) {
            structure.bootType = "UEFI Only";
        } else if (structure.hasElTorito) {
            structure.bootType = "Legacy BIOS (El Torito)";
        } else if (structure.isHybrid) {
            structure.bootType = "Hybrid ISO";
        } else {
            structure.bootType = "Data Only";
        }
        
        Logs::info("ISO Analysis Complete:");
        Logs::info("  Type: " + structure.bootType);
        Logs::info("  Hybrid: " + std::string(structure.isHybrid ? "Yes" : "No"));
        Logs::info("  UEFI: " + std::string(structure.hasUEFI ? "Yes" : "No"));
        Logs::info("  Legacy Boot: " + std::string(structure.hasLegacyBoot ? "Yes" : "No"));
        Logs::info("  Embedded Partitions: " + std::to_string(structure.embeddedPartitions.size()));
        
        return structure;
    }
    
    int SmartAnalyzer::calculateRequiredPartitions(const ISOStructure& structure, 
                                                   bool withPersistence) {
        int partitions = 1; // Minimum 1 for ISO data
        
        // Check if ISO has embedded partitions that should be preserved
        if (structure.isHybrid && !structure.embeddedPartitions.empty()) {
            partitions = structure.embeddedPartitions.size();
        }
        
        // Multi-boot ISOs need separate partitions for UEFI and Legacy
        if (structure.isMultiBoot) {
            partitions = std::max(partitions, 2);
        }
        
        // UEFI systems typically need EFI system partition
        if (structure.hasUEFI && !structure.isHybrid) {
            partitions = std::max(partitions, 2); // ESP + Data
        }
        
        // Add persistence partition if requested
        if (withPersistence) {
            partitions++;
        }
        
        Logs::info("Required partitions: " + std::to_string(partitions));
        return partitions;
    }
    
    std::string SmartAnalyzer::getRecommendedStrategy(const ISOStructure& structure) {
        if (structure.isHybrid) {
            return "Hybrid ISO detected - will preserve existing partition structure";
        } else if (structure.isMultiBoot) {
            return "Multi-boot ISO - creating separate UEFI and Legacy partitions";
        } else if (structure.hasUEFI) {
            return "UEFI ISO - creating EFI system partition";
        } else if (structure.hasElTorito) {
            return "Legacy bootable ISO - creating single bootable partition";
        } else {
            return "Data ISO - creating single data partition";
        }
    }
    
    bool SmartAnalyzer::checkElTorito(const std::string& isoPath) {
        std::ifstream file(isoPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Check sector 17 (offset 34816) for El Torito
        file.seekg(34816);
        char buffer[2048];
        file.read(buffer, 2048);
        
        std::string content(buffer, 2048);
        bool hasElTorito = (content.find("EL TORITO") != std::string::npos ||
                           content.find("BOOT CATALOG") != std::string::npos ||
                           content.find("BOOTABLE") != std::string::npos);
        
        file.close();
        return hasElTorito;
    }
    
    bool SmartAnalyzer::checkUEFI(const std::string& isoPath) {
        std::ifstream file(isoPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Check for EFI boot files in the ISO
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0);
        
        // Read first 1MB to scan for EFI signatures
        size_t scanSize = std::min(fileSize, static_cast<size_t>(1024 * 1024));
        char* buffer = new char[scanSize];
        file.read(buffer, scanSize);
        
        std::string content(buffer, scanSize);
        delete[] buffer;
        
        bool hasUEFI = (content.find("EFI/BOOT") != std::string::npos ||
                       content.find("efi/boot") != std::string::npos ||
                       content.find("BOOTX64.EFI") != std::string::npos ||
                       content.find("bootx64.efi") != std::string::npos ||
                       content.find("BOOTIA32.EFI") != std::string::npos);
        
        file.close();
        return hasUEFI;
    }
    
    bool SmartAnalyzer::checkHybridISO(const std::string& isoPath) {
        std::ifstream file(isoPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Check for MBR at beginning
        char mbr[512];
        file.read(mbr, 512);
        
        bool hasMBRSignature = (static_cast<uint8_t>(mbr[510]) == 0x55 && 
                               static_cast<uint8_t>(mbr[511]) == 0xAA);
        
        if (!hasMBRSignature) {
            file.close();
            return false;
        }
        
        // Check for partition table entries
        bool hasPartitions = false;
        for (int i = 446; i < 510; i += 16) {
            if (mbr[i] != 0 || mbr[i+4] != 0) {
                hasPartitions = true;
                break;
            }
        }
        
        // Check for ISO 9660 signature
        file.seekg(32768);
        char isoSig[6];
        file.read(isoSig, 6);
        bool hasISO9660 = (std::string(isoSig, 5) == "CD001");
        
        file.close();
        
        return hasMBRSignature && hasPartitions && hasISO9660;
    }
    
    std::vector<PartitionInfo> SmartAnalyzer::extractEmbeddedPartitions(
        const std::string& isoPath) {
        
        std::vector<PartitionInfo> partitions;
        
        std::ifstream file(isoPath, std::ios::binary);
        if (!file.is_open()) return partitions;
        
        // Read MBR
        char mbr[512];
        file.read(mbr, 512);
        
        // Check MBR signature
        if (static_cast<uint8_t>(mbr[510]) != 0x55 || 
            static_cast<uint8_t>(mbr[511]) != 0xAA) {
            file.close();
            return partitions;
        }
        
        // Parse partition table entries
        for (int i = 0; i < 4; i++) {
            int offset = 446 + (i * 16);
            
            uint8_t status = mbr[offset];
            uint8_t type = mbr[offset + 4];
            
            if (type == 0x00) continue; // Empty partition
            
            uint32_t startLBA = *reinterpret_cast<uint32_t*>(&mbr[offset + 8]);
            uint32_t sectorCount = *reinterpret_cast<uint32_t*>(&mbr[offset + 12]);
            
            PartitionInfo info;
            info.startLBA = startLBA;
            info.sectorCount = sectorCount;
            info.type = type;
            info.bootable = (status == 0x80);
            
            // Determine filesystem type from partition type
            switch (type) {
                case 0x0B:
                case 0x0C:
                    info.filesystem = "FAT32";
                    break;
                case 0x83:
                    info.filesystem = "Linux";
                    break;
                case 0xEF:
                    info.filesystem = "EFI";
                    break;
                default:
                    info.filesystem = "Unknown";
            }
            
            partitions.push_back(info);
        }
        
        file.close();
        return partitions;
    }
    
    std::vector<std::string> SmartAnalyzer::findBootFiles(const std::string& isoPath) {
        std::vector<std::string> bootFiles;
        
        std::ifstream file(isoPath, std::ios::binary);
        if (!file.is_open()) return bootFiles;
        
        // Read first 2MB for boot file detection
        const size_t scanSize = 2 * 1024 * 1024;
        char* buffer = new char[scanSize];
        file.read(buffer, scanSize);
        
        std::string content(buffer, scanSize);
        delete[] buffer;
        
        // Common boot files
        std::vector<std::string> patterns = {
            "ISOLINUX.BIN", "isolinux.bin",
            "SYSLINUX.BIN", "syslinux.bin",
            "BOOTX64.EFI", "bootx64.efi",
            "BOOTIA32.EFI", "bootia32.efi",
            "GRUBX64.EFI", "grubx64.efi",
            "GRUB.CFG", "grub.cfg",
            "VMLINUZ", "vmlinuz",
            "INITRD", "initrd"
        };
        
        for (const auto& pattern : patterns) {
            if (content.find(pattern) != std::string::npos) {
                bootFiles.push_back(pattern);
            }
        }
        
        file.close();
        return bootFiles;
    }
    
    BurnStrategy determineBurnStrategy(const ISOStructure& structure) {
        if (structure.isHybrid && !structure.embeddedPartitions.empty()) {
            return BurnStrategy::HYBRID_PRESERVE;
        } else if (structure.isMultiBoot || structure.embeddedPartitions.size() > 1) {
            return BurnStrategy::MULTIPART;
        } else if (structure.hasUEFI || structure.hasElTorito) {
            return BurnStrategy::SMART_EXTRACT;
        } else {
            return BurnStrategy::RAW_COPY;
        }
    }
}
