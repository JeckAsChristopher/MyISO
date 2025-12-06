#include "lib/iso_burner.hpp"
#include "lib/dev_handler.hpp"
#include "lib/persistence.hpp"
#include "lib/persistence_fallback.hpp"
#include "lib/fs_supports.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include "utils/colors.hpp"
#include "misc/version.hpp"
#include <iostream>
#include <string>
#include <unistd.h>

struct Options {
    std::string isoPath;
    std::string device;
    size_t persistenceSize = 0;
    FilesystemSupport::FSType fsType = FilesystemSupport::FSType::EXT4;
    bool usePersistence = false;
    bool useFastMode = false;
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
    std::cout << "  -v             Show version information\n";
    std::cout << "  -h             Show this help message\n\n";

    std::cout << Colors::bold("Examples:") << "\n";
    std::cout << "  MI -i ubuntu.iso -o /dev/sdb\n";
    std::cout << "  MI -i ubuntu.iso -p 4096 -f ext4 -o /dev/sdb\n";
    std::cout << "  MI -i linux.iso -p 2048 -o /dev/sdc -m\n\n";

    std::cout << Colors::yellow("Note: ") << "This tool requires root privileges (use sudo)\n";
}

bool parseArguments(int argc, char* argv[], Options& opts) {
    int opt;

    while ((opt = getopt(argc, argv, "i:o:p:f:mvh")) != -1) {
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
            case 'm':
                opts.useFastMode = true;
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

int main(int argc, char* argv[]) {
    Options opts;  // Moved here to fix scope issue

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

        Logs::info("ISO File: " + opts.isoPath);
        Logs::info("Target Device: " + opts.device);

        if (!DeviceHandler::validateDevice(opts.device)) {
            throw DeviceError(opts.device, "Invalid block device");
        }

        if (!ISOBurner::validateISO(opts.isoPath)) {
            throw FileError(opts.isoPath, "Invalid ISO file");
        }

        size_t deviceSize = DeviceHandler::getDeviceSize(opts.device);
        size_t isoSize = ISOBurner::getISOSize(opts.isoPath);

        Logs::info("Device size: " + std::to_string(deviceSize / (1024*1024)) + " MB");
        Logs::info("ISO size: " + std::to_string(isoSize / (1024*1024)) + " MB");

        if (isoSize > deviceSize) {
            throw DeviceError(opts.device, "Device too small for ISO");
        }

        std::cout << Colors::yellow("\nWARNING: All data on " + opts.device +
                     " will be destroyed!") << std::endl;
        std::cout << "Continue? (yes/no): ";

        std::string confirm;
        std::cin >> confirm;

        if (confirm != "yes") {
            Logs::info("Operation cancelled by user");
            return 0;
        }

        if (opts.usePersistence) {
            Logs::info("Persistence enabled: " +
                      std::to_string(opts.persistenceSize) + " MB (" +
                      FilesystemSupport::getFSName(opts.fsType) + ")");

            try {
                Persistence::setupPersistence(
                    opts.isoPath,
                    opts.device,
                    opts.persistenceSize,
                    opts.fsType
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
            DeviceHandler::unmountDevice(opts.device);

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
        Logs::fatal(e.what());
        return 1;
    } catch (const DeviceError& e) {
        ErrorHandler::handleFatalError(opts.device, e.what());
        return 1;
    } catch (const FileError& e) {
        Logs::fatal(e.what());
        return 1;
    } catch (const std::exception& e) {
        Logs::fatal("Unexpected error: " + std::string(e.what()));
        return 1;
    }
}
