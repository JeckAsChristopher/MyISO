#include "lib/persistence.hpp"
#include "lib/dev_handler.hpp"
#include "lib/iso_burner.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <cmath>

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
        
        std::string persistDevice = DeviceHandler::createPartition(device, sizeInMB);
        
        FilesystemSupport::formatPartition(persistDevice, fsType);
        
        DeviceHandler::syncDevice(device);
        
        Logs::success("Persistence partition created: " + persistDevice);
        return true;
    }
    
    bool setupPersistence(
        const std::string& isoPath,
        const std::string& device,
        size_t persistenceSizeMB,
        FilesystemSupport::FSType fsType
    ) {
        Logs::info("Setting up bootable USB with persistence");
        
        size_t isoSize = ISOBurner::getISOSize(isoPath);
        size_t deviceSize = DeviceHandler::getDeviceSize(device);
        
        size_t isoSizeMB = (isoSize / (1024 * 1024)) + 100;
        
        if (persistenceSizeMB < 512) {
            Logs::warning("Persistence size too small, setting to minimum 512 MB");
            persistenceSizeMB = 512;
        }
        
        size_t totalRequired = isoSizeMB + persistenceSizeMB;
        size_t deviceSizeMB = deviceSize / (1024 * 1024);
        
        if (totalRequired > deviceSizeMB) {
            throw FilesystemError("Not enough space on device for ISO + persistence");
        }
        
        DeviceHandler::unmountDevice(device);
        DeviceHandler::createPartitionTable(device);
        
        std::string part1 = DeviceHandler::createPartition(device, isoSizeMB);
        
        Logs::info("Burning ISO to first partition");
        ISOBurner::burnISO(isoPath, part1, ISOBurner::BurnMode::RAW);
        
        createPersistencePartition(device, persistenceSizeMB, fsType);
        
        DeviceHandler::syncDevice(device);
        
        Logs::success("Bootable USB with persistence created successfully");
        return true;
    }
    
    size_t calculateOptimalSize(size_t isoSize, size_t deviceSize) {
        size_t isoSizeMB = isoSize / (1024 * 1024);
        size_t deviceSizeMB = deviceSize / (1024 * 1024);
        
        size_t availableSpace = deviceSizeMB - isoSizeMB - 100;
        
        if (availableSpace < 512) return 0;
        
        return std::min(availableSpace, static_cast<size_t>(8192));
    }
}
