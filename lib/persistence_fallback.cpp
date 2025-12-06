#include "lib/persistence_fallback.hpp"
#include "lib/iso_burner.hpp"
#include "lib/dev_handler.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <cstdlib>
#include <fstream>

namespace PersistenceFallback {
    
    bool createFileBased(
        const std::string& mountPoint,
        size_t sizeInMB,
        const std::string& label
    ) {
        Logs::info("Creating file-based persistence (" + std::to_string(sizeInMB) + " MB)");
        
        std::string persistFile = mountPoint + "/" + label;
        
        std::string ddCmd = "dd if=/dev/zero of=" + persistFile + 
                            " bs=1M count=" + std::to_string(sizeInMB) + 
                            " 2>/dev/null";
        
        Logs::info("Allocating persistence file...");
        int result = system(ddCmd.c_str());
        
        if (result != 0) {
            throw FilesystemError("Failed to create persistence file");
        }
        
        std::string mkfsCmd = "mkfs.ext4 -F -L " + label + " " + persistFile + 
                              " > /dev/null 2>&1";
        
        Logs::info("Formatting persistence file...");
        result = system(mkfsCmd.c_str());
        
        if (result != 0) {
            throw FilesystemError("Failed to format persistence file");
        }
        
        Logs::success("File-based persistence created: " + persistFile);
        return true;
    }
    
    bool setupFallbackPersistence(
        const std::string& isoPath,
        const std::string& device,
        size_t persistenceSizeMB
    ) {
        Logs::info("Setting up fallback persistence method");
        
        DeviceHandler::unmountDevice(device);
        
        Logs::info("Burning ISO to device");
        ISOBurner::burnISO(isoPath, device, ISOBurner::BurnMode::RAW);
        
        DeviceHandler::syncDevice(device);
        
        Logs::info("Mounting device to create persistence file");
        std::string mountPoint = "/tmp/myiso_mount";
        system(("mkdir -p " + mountPoint).c_str());
        
        std::string mountCmd = "mount " + device + "1 " + mountPoint + " 2>/dev/null";
        int result = system(mountCmd.c_str());
        
        if (result != 0) {
            Logs::warning("Could not mount device for file-based persistence");
            return false;
        }
        
        try {
            createFileBased(mountPoint, persistenceSizeMB);
            
            system(("umount " + mountPoint).c_str());
            system(("rm -rf " + mountPoint).c_str());
            
            DeviceHandler::syncDevice(device);
            
            Logs::success("Fallback persistence setup complete");
            return true;
            
        } catch (...) {
            system(("umount " + mountPoint + " 2>/dev/null").c_str());
            system(("rm -rf " + mountPoint).c_str());
            throw;
        }
    }
}
