#include "lib/dev_handler.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fstream>
#include <sys/stat.h>
#include <sys/mount.h>
#include <mntent.h>
#include <unistd.h>
#include <cstdlib>

namespace DeviceHandler {
    
    bool validateDevice(const std::string& device) {
        struct stat st;
        if (stat(device.c_str(), &st) != 0) {
            return false;
        }
        
        return S_ISBLK(st.st_mode);
    }
    
    bool isDeviceMounted(const std::string& device) {
        FILE* mtab = setmntent("/etc/mtab", "r");
        if (!mtab) return false;
        
        struct mntent* entry;
        bool mounted = false;
        
        while ((entry = getmntent(mtab)) != nullptr) {
            if (device.find(entry->mnt_fsname) == 0) {
                mounted = true;
                break;
            }
        }
        
        endmntent(mtab);
        return mounted;
    }
    
    bool unmountDevice(const std::string& device) {
        if (!isDeviceMounted(device)) {
            return true;
        }
        
        Logs::info("Unmounting " + device);
        
        std::string cmd = "umount " + device + "* 2>/dev/null";
        system(cmd.c_str());
        
        sleep(1);
        
        if (isDeviceMounted(device)) {
            Logs::warning("Failed to unmount device cleanly, forcing...");
            cmd = "umount -l " + device + "* 2>/dev/null";
            system(cmd.c_str());
        }
        
        return true;
    }
    
    size_t getDeviceSize(const std::string& device) {
        std::string sizeFile = "/sys/class/block/" + 
                               device.substr(device.find_last_of('/') + 1) + 
                               "/size";
        
        std::ifstream file(sizeFile);
        if (!file.is_open()) {
            throw DeviceError(device, "Cannot read device size");
        }
        
        size_t sectors;
        file >> sectors;
        
        return sectors * 512;
    }
    
    bool createPartitionTable(const std::string& device) {
        Logs::info("Creating partition table on " + device);
        
        std::string cmd = "parted -s " + device + " mklabel msdos 2>/dev/null";
        int result = system(cmd.c_str());
        
        if (result != 0) {
            throw DeviceError(device, "Failed to create partition table");
        }
        
        return true;
    }
    
    std::string createPartition(const std::string& device, size_t sizeInMB) {
        Logs::info("Creating partition of " + std::to_string(sizeInMB) + " MB");
        
        std::string cmd = "parted -s " + device + 
                          " mkpart primary ext4 1MiB " + 
                          std::to_string(sizeInMB) + "MiB 2>/dev/null";
        
        int result = system(cmd.c_str());
        
        if (result != 0) {
            throw DeviceError(device, "Failed to create partition");
        }
        
        sleep(1);
        system(("partprobe " + device + " 2>/dev/null").c_str());
        sleep(1);
        
        return device + "1";
    }
    
    bool syncDevice(const std::string& device) {
        Logs::info("Syncing device buffers...");
        sync();
        
        std::string cmd = "blockdev --flushbufs " + device + " 2>/dev/null";
        system(cmd.c_str());
        
        return true;
    }
}
