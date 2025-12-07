#include "lib/iso_burner.hpp"
#include "lib/dev_handler.hpp"
#include "lib/persistence.hpp"
#include "lib/persistence_fallback.hpp"
#include "lib/fs_supports.hpp"
#include "lib/errors.hpp"
#include "lib/mbr_gpt.hpp"
#include "utils/logs.hpp"
#include "utils/colors.hpp"
#include "misc/version.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <getopt.h>
#include <unistd.h>
#include <regex>

struct Options {
    std::string isoPath;
    std::string device;
    size_t persistenceSize = 0;
    FilesystemSupport::FSType fsType = FilesystemSupport::FSType::EXT4;
    bool usePersistence = false;
    bool useFastMode = false;
    bool dryRun = false;
    bool aggressiveInfo = false;
    bool forceOperation = false;
    BootStructures::TableType tableType = BootStructures::TableType::MBR;
};

void printUsage() {
    std::cout << Colors::bold("Usage:") << " MI [OPTIONS]\n\n";
    std::cout << Colors::cyan("Options:") << "\n";
    std::cout << "  -i <file>      Input ISO file\n";
    std::cout << "  -o <device>    Output device (e.g., /dev/sdX)\n";
    std::cout << "  -p <size>      Enable persistence with size in MB\n";
    std::cout << "  -f <fs>        Filesystem type for persistence\n";
    std::cout << "                 (ext4, ntfs, exfat, FAT32, FAT64)\n";
    std::cout << "  -m             Use fast mode for ISO burning\n";
    std::cout << "  -t <type>      Partition table type (mbr or gpt)\n";
    std::cout << "                 If not specified, will prompt interactively\n";
    std::cout << "  --dry-run      Show all information without performing operations\n";
    std::cout << "  -asi           Show aggressive system info (quick, non-comprehensive)\n";
    std::cout << "  --force        Force operation, bypass warnings\n";
    std::cout << "  -v             Show version information\n";
    std::cout << "  -h             Show this help message\n\n";
    
    std::cout << Colors::bold("Examples:") << "\n";
    std::cout << "  MI -i ubuntu.iso -o /dev/sdb\n";
    std::cout << "  MI -i ubuntu.iso -p 4096 -f ext4 -o /dev/sdb --dry-run\n";
    std::cout << "  MI -i linux.iso -p 2048 -o /dev/sdc -m -t gpt --force\n";
    std::cout << "  MI -i debian.iso -o /dev/sdb -asi\n\n";
    
    std::cout << Colors::yellow("Note: ") << "This tool requires root privileges (use sudo)\n";
    std::cout << Colors::yellow("      Device must be whole disk (e.g., /dev/sdb), not partition (e.g., /dev/sdb1)\n");
}

bool parseArguments(int argc, char* argv[], Options& opts) {
    int opt;
    
    static struct option long_options[] = {
        {"dry-run", no_argument, 0, 'd'},
        {"force", no_argument, 0, 'F'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "i:o:p:f:t:mvha", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                opts.isoPath = optarg;
                break;
            case 'o':
                opts.device = optarg;
                break;
            case 'p':
                opts.usePersistence = true;
                try {
                    opts.persistenceSize = std::stoul(optarg);
                } catch (...) {
                    Logs::error("Invalid persistence size");
                    return false;
                }
                break;
            case 'f':
                opts.fsType = FilesystemSupport::parseFSType(optarg);
                if (!FilesystemSupport::isSupported(opts.fsType)) {
                    Logs::error("Unsupported filesystem: " + std::string(optarg));
                    std::cout << "Supported filesystems: ";
                    for (const auto& fs : FilesystemSupport::getSupportedFilesystems()) {
                        std::cout << fs << " ";
                    }
                    std::cout << std::endl;
                    return false;
                }
                break;
            case 't': {
                std::string tableTypeStr = optarg;
                std::transform(tableTypeStr.begin(), tableTypeStr.end(), 
                             tableTypeStr.begin(), ::tolower);
                if (tableTypeStr == "mbr") {
                    opts.tableType = BootStructures::TableType::MBR;
                } else if (tableTypeStr == "gpt") {
                    opts.tableType = BootStructures::TableType::GPT;
                } else {
                    Logs::error("Invalid partition table type. Use 'mbr' or 'gpt'");
                    return false;
                }
                break;
            }
            case 'm':
                opts.useFastMode = true;
                break;
            case 'd':
                opts.dryRun = true;
                break;
            case 'a':
                opts.aggressiveInfo = true;
                break;
            case 'F':
                opts.forceOperation = true;
                break;
            case 'v':
                Version::printVersion();
                exit(0);
            case 'h':
                printUsage();
                exit(0);
            default:
                printUsage();
                return false;
        }
    }
    
    if (opts.isoPath.empty() || opts.device.empty()) {
        Logs::error("Both -i (input ISO) and -o (output device) are required");
        printUsage();
        return false;
    }
    
    if (!opts.usePersistence && opts.fsType != FilesystemSupport::FSType::EXT4) {
        Logs::error("-f (filesystem) option only works with -p (persistence)");
        return false;
    }
    
    return true;
}

bool isPartitionDevice(const std::string& device) {
    // Check if device ends with a number (partition)
    if (device.empty()) return false;
    
    char lastChar = device.back();
    return (lastChar >= '0' && lastChar <= '9');
}

std::string getBaseDevice(const std::string& device) {
    std::string base = device;
    while (!base.empty() && base.back() >= '0' && base.back() <= '9') {
        base.pop_back();
    }
    return base;
}

void showDryRunInfo(const Options& opts, size_t deviceSizeMB, size_t isoSizeMB, 
                    const std::string& isoType) {
    std::cout << "\n" << Colors::bold(Colors::cyan("=== DRY RUN MODE - NO CHANGES WILL BE MADE ===")) << "\n\n";
    
    std::cout << Colors::bold("Input Information:") << "\n";
    std::cout << "  ISO File: " << opts.isoPath << "\n";
    std::cout << "  ISO Size: " << isoSizeMB << " MB\n";
    std::cout << "  ISO Type: " << isoType << "\n";
    std::cout << "  Target Device: " << opts.device << "\n";
    std::cout << "  Device Size: " << deviceSizeMB << " MB (" << (deviceSizeMB/1024) << " GB)\n\n";
    
    std::cout << Colors::bold("Operation Details:") << "\n";
    std::cout << "  Partition Table: " << (opts.tableType == BootStructures::TableType::MBR ? "MBR" : "GPT") << "\n";
    std::cout << "  Burn Mode: " << (opts.useFastMode ? "Fast (Zero-Copy)" : "Raw (Standard)") << "\n";
    
    if (opts.usePersistence) {
        std::cout << "  Persistence: Enabled\n";
        std::cout << "  Persistence Size: " << opts.persistenceSize << " MB\n";
        std::cout << "  Persistence Filesystem: " << FilesystemSupport::getFSName(opts.fsType) << "\n";
    } else {
        std::cout << "  Persistence: Disabled\n";
    }
    
    std::cout << "\n" << Colors::bold("Planned Operations:") << "\n";
    std::cout << "  1. Unmount all partitions on " << opts.device << "\n";
    std::cout << "  2. Create " << (opts.tableType == BootStructures::TableType::MBR ? "MBR" : "GPT") << " partition table\n";
    
    if (opts.usePersistence) {
        std::cout << "  3. Create partition 1: FAT32 (" << isoSizeMB << " MB)\n";
        std::cout << "  4. Burn ISO to partition 1\n";
        std::cout << "  5. Create partition 2: " << FilesystemSupport::getFSName(opts.fsType) 
                  << " (" << opts.persistenceSize << " MB)\n";
        std::cout << "  6. Install bootloader (SYSLINUX/GRUB)\n";
    } else {
        std::cout << "  3. Burn ISO directly to device\n";
        std::cout << "  4. Install bootloader (SYSLINUX/GRUB)\n";
    }
    
    std::cout << "  " << (opts.usePersistence ? "7" : "5") << ". Sync and finalize\n";
    
    size_t totalUsed = isoSizeMB + (opts.usePersistence ? opts.persistenceSize : 0) + 100;
    size_t remaining = deviceSizeMB - totalUsed;
    
    std::cout << "\n" << Colors::bold("Space Analysis:") << "\n";
    std::cout << "  ISO: " << isoSizeMB << " MB\n";
    if (opts.usePersistence) {
        std::cout << "  Persistence: " << opts.persistenceSize << " MB\n";
    }
    std::cout << "  Overhead: ~100 MB\n";
    std::cout << "  Total Used: " << totalUsed << " MB\n";
    std::cout << "  Remaining: " << remaining << " MB\n";
    std::cout << "  Usage: " << ((totalUsed * 100) / deviceSizeMB) << "%\n";
    
    std::cout << "\n" << Colors::green("All checks passed. Ready to proceed with actual operation.") << "\n";
    std::cout << Colors::yellow("Remove --dry-run flag to perform the actual operation.") << "\n\n";
}

void showAggressiveInfo(const Options& opts) {
    std::cout << Colors::bold(Colors::cyan("\n=== AGGRESSIVE SYSTEM INFO ===\n"));
    std::cout << "ISO: " << opts.isoPath << "\n";
    std::cout << "DEV: " << opts.device << "\n";
    std::cout << "MODE: " << (opts.useFastMode ? "FAST" : "RAW") << "\n";
    std::cout << "PTABLE: " << (opts.tableType == BootStructures::TableType::MBR ? "MBR" : "GPT") << "\n";
    if (opts.usePersistence) {
        std::cout << "PERSIST: " << opts.persistenceSize << "MB " 
                  << FilesystemSupport::getFSName(opts.fsType) << "\n";
    }
    std::cout << "FORCE: " << (opts.forceOperation ? "YES" : "NO") << "\n";
    std::cout << Colors::cyan("===========================\n\n");
}

BootStructures::TableType promptPartitionTableType() {
    std::cout << "\n" << Colors::bold(Colors::cyan("╔════════════════════════════════════════════════════════════════╗")) << std::endl;
    std::cout << Colors::bold(Colors::cyan("║        PARTITION TABLE SELECTION                              ║")) << std::endl;
    std::cout << Colors::bold(Colors::cyan("╚════════════════════════════════════════════════════════════════╝")) << std::endl;
    std::cout << "\n";
    
    std::cout << Colors::yellow("Please choose the installation partition table type.") << std::endl;
    std::cout << Colors::yellow("This is needed by the BIOS to properly boot your system.") << std::endl;
    std::cout << "\n";
    std::cout << Colors::cyan("If you don't know what this is, read the manual at:") << std::endl;
    std::cout << Colors::bold("https://wiki.archlinux.org/title/Partitioning#Partition_table") << std::endl;
    std::cout << "\n";
    
    std::cout << Colors::green("[1]. MBR (Master Boot Record)") << std::endl;
    std::cout << "     " << Colors::white("• Compatible with older systems (BIOS)") << std::endl;
    std::cout << "     " << Colors::white("• Maximum 4 primary partitions") << std::endl;
    std::cout << "     " << Colors::white("• Supports disks up to 2TB") << std::endl;
    std::cout << "     " << Colors::white("• Recommended for maximum compatibility") << std::endl;
    std::cout << "\n";
    
    std::cout << Colors::green("[2]. GPT (GUID Partition Table)") << std::endl;
    std::cout << "     " << Colors::white("• Required for UEFI systems") << std::endl;
    std::cout << "     " << Colors::white("• Supports 128 partitions") << std::endl;
    std::cout << "     " << Colors::white("• Supports disks larger than 2TB") << std::endl;
    std::cout << "     " << Colors::white("• Recommended for modern systems") << std::endl;
    std::cout << "\n";
    
    std::cout << Colors::bold("Choose [1/2]: ");
    std::cout.flush();
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "1" || choice == "mbr" || choice == "MBR") {
        std::cout << "\n" << Colors::green("✓ Selected: MBR (Master Boot Record)") << std::endl;
        return BootStructures::TableType::MBR;
    } else if (choice == "2" || choice == "gpt" || choice == "GPT") {
        std::cout << "\n" << Colors::green("✓ Selected: GPT (GUID Partition Table)") << std::endl;
        return BootStructures::TableType::GPT;
    } else {
        std::cout << "\n" << Colors::yellow("Invalid choice, defaulting to MBR for compatibility") << std::endl;
        return BootStructures::TableType::MBR;
    }
}

int main(int argc, char* argv[]) {
    Options opts;
    
    try {
        Version::printBanner();
        
        if (argc < 2) {
            printUsage();
            return 1;
        }
        
        if (!parseArguments(argc, argv, opts)) {
            return 1;
        }
        
        ErrorHandler::checkPrivileges();
        
        // Show aggressive info if requested
        if (opts.aggressiveInfo) {
            showAggressiveInfo(opts);
            if (!opts.dryRun) {
                return 0; // Exit after showing info unless dry-run
            }
        }
        
        Logs::info("ISO File: " + opts.isoPath);
        Logs::info("Target Device: " + opts.device);
        
        // Validate device is not a partition
        if (isPartitionDevice(opts.device)) {
            std::string baseDevice = getBaseDevice(opts.device);
            Logs::fatal("Fatal Error: The target device is incomplete.");
            std::cerr << Colors::red("  You specified: " + opts.device) << std::endl;
            std::cerr << Colors::green("  Try instead: " + baseDevice) << std::endl;
            std::cerr << Colors::yellow("  Just remove the number at the end.") << std::endl;
            return 1;
        }
        
        if (!DeviceHandler::validateDevice(opts.device)) {
            throw DeviceError(opts.device, "Invalid block device");
        }
        
        if (!ISOBurner::validateISO(opts.isoPath)) {
            throw FileError(opts.isoPath, "Invalid ISO file");
        }
        
        std::string isoType = ISOBurner::detectISOType(opts.isoPath);
        Logs::info("ISO Type: " + isoType);
        
        size_t deviceSize = DeviceHandler::getDeviceSize(opts.device);
        size_t isoSize = ISOBurner::getISOSize(opts.isoPath);
        
        size_t deviceSizeMB = deviceSize / (1024 * 1024);
        size_t isoSizeMB = isoSize / (1024 * 1024);
        
        Logs::info("Device size: " + std::to_string(deviceSizeMB) + " MB (" + 
                  std::to_string(deviceSize / (1024*1024*1024)) + " GB)");
        Logs::info("ISO size: " + std::to_string(isoSizeMB) + " MB");
        
        if (isoSize > deviceSize) {
            throw DeviceError(opts.device, "Device too small for ISO");
        }
        
        // Check available space for persistence if requested
        if (opts.usePersistence) {
            size_t requiredSpace = isoSizeMB + opts.persistenceSize + 200;
            size_t availableForPersistence = deviceSizeMB - isoSizeMB - 200;
            
            Logs::info("Available space for persistence: " + 
                      std::to_string(availableForPersistence) + " MB");
            
            if (requiredSpace > deviceSizeMB) {
                std::string errorMsg = "Insufficient storage for requested persistence\n";
                errorMsg += "  Device: " + std::to_string(deviceSizeMB) + " MB\n";
                errorMsg += "  ISO: " + std::to_string(isoSizeMB) + " MB\n";
                errorMsg += "  Requested persistence: " + std::to_string(opts.persistenceSize) + " MB\n";
                errorMsg += "  Required: " + std::to_string(requiredSpace) + " MB\n";
                errorMsg += "  Shortage: " + std::to_string(requiredSpace - deviceSizeMB) + " MB\n";
                
                if (availableForPersistence >= 512) {
                    errorMsg += "\n  Maximum persistence available: " + 
                               std::to_string(availableForPersistence) + " MB";
                    errorMsg += "\n\nTry: MI -i " + opts.isoPath + " -p " + 
                               std::to_string(availableForPersistence) + " -f " + 
                               FilesystemSupport::getFSName(opts.fsType) + " -o " + opts.device;
                } else {
                    errorMsg += "\n  Device too small for persistence (minimum 512 MB needed)";
                }
                
                throw FilesystemError(errorMsg);
            }
        }
        
        // Prompt for partition table type if not specified via command line
        std::cout << "\n";
        opts.tableType = promptPartitionTableType();
        std::cout << "\n";
        
        // Show dry-run information and exit if requested
        if (opts.dryRun) {
            showDryRunInfo(opts, deviceSizeMB, isoSizeMB, isoType);
            return 0;
        }
        
        std::cout << Colors::yellow("\nWARNING: All data on " + opts.device + 
                     " will be destroyed!") << std::endl;
        std::cout << "Continue? (yes/no): ";
        
        std::string confirm;
        std::cin >> confirm;
        
        if (confirm != "yes" && !opts.forceOperation) {
            Logs::info("Operation cancelled by user");
            return 0;
        }
        
        if (opts.forceOperation && confirm != "yes") {
            Logs::warning("Proceeding with --force flag");
        }
        
        if (opts.usePersistence) {
            Logs::info("Persistence enabled: " + 
                      std::to_string(opts.persistenceSize) + " MB (" +
                      FilesystemSupport::getFSName(opts.fsType) + ")");
            Logs::info("Partition table: " + 
                      std::string(opts.tableType == BootStructures::TableType::MBR ? "MBR" : "GPT"));
            
            try {
                Persistence::setupPersistence(
                    opts.isoPath,
                    opts.device,
                    opts.persistenceSize,
                    opts.fsType,
                    opts.tableType
                );
            } catch (const std::exception& e) {
                Logs::warning("Primary persistence method failed: " + 
                             std::string(e.what()));
                Logs::info("Attempting fallback method...");
                
                PersistenceFallback::setupFallbackPersistence(
                    opts.isoPath,
                    opts.device,
                    opts.persistenceSize
                );
            }
        } else {
            Logs::info("Starting standard ISO burn without persistence");
            
            DeviceHandler::unmountDevice(opts.device);
            
            Logs::info("Wiping existing data on device");
            DeviceHandler::wipeDevice(opts.device);
            
            ISOBurner::BurnMode mode = opts.useFastMode ? 
                ISOBurner::BurnMode::FAST : ISOBurner::BurnMode::RAW;
            
            ISOBurner::burnISO(opts.isoPath, opts.device, mode);
            DeviceHandler::syncDevice(opts.device);
        }
        
        std::cout << "\n" << Colors::green(Colors::bold("✓ SUCCESS!")) << std::endl;
        Logs::success("Bootable USB created successfully!");
        Logs::info("You can now safely remove " + opts.device);
        
        return 0;
        
    } catch (const PermissionError& e) {
        return 1;
    } catch (const DeviceError& e) {
        std::string device = opts.device.empty() ? "unknown" : opts.device;
        ErrorHandler::handleFatalError(device, e.what());
        return 1;
    } catch (const FileError& e) {
        Logs::fatal(e.what());
        return 1;
    } catch (const FilesystemError& e) {
        Logs::fatal(std::string(e.what()));
        return 1;
    } catch (const std::exception& e) {
        Logs::fatal("Unexpected error: " + std::string(e.what()));
        return 1;
    }
}
