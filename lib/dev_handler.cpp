#include "lib/dev_handler.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fstream>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <mntent.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

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
    
    bool wipeDevice(const std::string& device) {
        Logs::info("Wiping device " + device + " (clearing all partition data)");
        
        int fd = open(device.c_str(), O_WRONLY | O_SYNC);
        if (fd < 0) {
            throw DeviceError(device, "Cannot open device for wiping");
        }
        
        // Zero out first 10MB (MBR, GPT, partition tables, filesystem signatures)
        const size_t WIPE_SIZE = 10 * 1024 * 1024;
        const size_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
        
        uint8_t* zeros = new uint8_t[BUFFER_SIZE];
        memset(zeros, 0, BUFFER_SIZE);
        
        size_t totalWritten = 0;
        while (totalWritten < WIPE_SIZE) {
            ssize_t written = write(fd, zeros, BUFFER_SIZE);
            if (written < 0) {
                delete[] zeros;
                close(fd);
                throw DeviceError(device, "Failed to wipe device");
            }
            totalWritten += written;
        }
        
        // Also zero out last 10MB (backup GPT)
        uint64_t deviceSize;
        if (ioctl(fd, BLKGETSIZE64, &deviceSize) == 0) {
            off_t endPosition = deviceSize - WIPE_SIZE;
            if (lseek(fd, endPosition, SEEK_SET) == endPosition) {
                totalWritten = 0;
                while (totalWritten < WIPE_SIZE) {
                    ssize_t written = write(fd, zeros, BUFFER_SIZE);
                    if (written < 0) break;
                    totalWritten += written;
                }
            }
        }
        
        delete[] zeros;
        fsync(fd);
        close(fd);
        
        // Force kernel to re-read partition table
        fd = open(device.c_str(), O_RDONLY);
        if (fd >= 0) {
            ioctl(fd, BLKRRPART);
            close(fd);
        }
        
        sleep(1);
        
        Logs::success("Device wiped successfully");
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
        
        // Calculate sector positions (sector size = 512 bytes)
        uint32_t startSector = 2048; // Start at 1MB for proper alignment
        uint32_t sizeInSectors = (sizeInMB * 1024 * 1024) / 512;
        uint32_t endSector = startSector + sizeInSectors - 1;
        
        // Use sfdisk for reliable partition creation
        std::string sfdiskCmd = "echo 'start=" + std::to_string(startSector) + 
                                ", size=" + std::to_string(sizeInSectors) + 
                                ", type=c, bootable' | sfdisk " + device + " 2>&1";
        
        Logs::debug("Executing: " + sfdiskCmd);
        
        FILE* pipe = popen(sfdiskCmd.c_str(), "r");
        if (!pipe) {
            throw DeviceError(device, "Failed to execute sfdisk command");
        }
        
        char buffer[256];
        std::string output;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        int result = pclose(pipe);
        
        if (result != 0) {
            Logs::error("sfdisk output: " + output);
            throw DeviceError(device, "Failed to create partition with sfdisk");
        }
        
        // Force kernel to re-read partition table
        sleep(1);
        
        int fd = open(device.c_str(), O_RDONLY);
        if (fd >= 0) {
            if (ioctl(fd, BLKRRPART) < 0) {
                Logs::warning("Failed to refresh partition table, trying partprobe");
            }
            close(fd);
        }
        
        system(("partprobe " + device + " 2>/dev/null").c_str());
        sleep(2);
        
        // Determine partition name
        std::string partitionDevice;
        if (device.find("nvme") != std::string::npos || 
            device.find("mmcblk") != std::string::npos) {
            partitionDevice = device + "p1";
        } else {
            partitionDevice = device + "1";
        }
        
        // Verify partition exists
        struct stat st;
        int attempts = 0;
        while (attempts < 10 && stat(partitionDevice.c_str(), &st) != 0) {
            sleep(1);
            attempts++;
            system(("partprobe " + device + " 2>/dev/null").c_str());
        }
        
        if (stat(partitionDevice.c_str(), &st) != 0) {
            throw DeviceError(device, "Partition " + partitionDevice + " not found after creation");
        }
        
        Logs::success("Partition created: " + partitionDevice);
        return partitionDevice;
    }
    
    bool syncDevice(const std::string& device) {
        Logs::info("Syncing device buffers...");
        sync();
        
        std::string cmd = "blockdev --flushbufs " + device + " 2>/dev/null";
        system(cmd.c_str());
        
        return true;
    }
}
