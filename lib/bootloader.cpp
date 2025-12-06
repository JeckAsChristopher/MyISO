#include "lib/bootloader.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include "utils/progress_bar.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace Bootloader {
    
    BootloaderInstaller::BootloaderInstaller(const std::string& dev, BootType type)
        : device(dev), mountPoint("/tmp/myiso_boot_" + std::to_string(getpid())), 
          bootType(type) {
    }
    
    BootloaderInstaller::~BootloaderInstaller() {
        if (std::filesystem::exists(mountPoint)) {
            umount(mountPoint.c_str());
            std::filesystem::remove_all(mountPoint);
        }
    }
    
    bool BootloaderInstaller::detectBootType(const std::string& isoPath) {
        Logs::info("Detecting bootloader type from ISO");
        
        int fd = open(isoPath.c_str(), O_RDONLY);
        if (fd < 0) return false;
        
        char buffer[32768];
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        close(fd);
        
        if (bytes < 0) return false;
        
        std::string content(buffer, bytes);
        
        if (content.find("ISOLINUX") != std::string::npos ||
            content.find("SYSLINUX") != std::string::npos) {
            bootType = BootType::SYSLINUX;
            Logs::info("Detected SYSLINUX/ISOLINUX bootloader");
            return true;
        }
        
        if (content.find("GRUB") != std::string::npos) {
            bootType = BootType::GRUB;
            Logs::info("Detected GRUB bootloader");
            return true;
        }
        
        bootType = BootType::SYSLINUX;
        Logs::info("Using SYSLINUX as default bootloader");
        return true;
    }
    
    bool BootloaderInstaller::extractISO(const std::string& isoPath, 
                                         const std::string& destination) {
        Logs::info("Extracting ISO contents to " + destination);
        
        std::string loopDevice = "/dev/loop0";
        for (int i = 0; i < 8; i++) {
            loopDevice = "/dev/loop" + std::to_string(i);
            if (access(loopDevice.c_str(), F_OK) == 0) {
                struct stat st;
                if (stat(loopDevice.c_str(), &st) == 0) {
                    continue;
                }
                break;
            }
        }
        
        std::string losetup = "losetup " + loopDevice + " " + isoPath + " 2>/dev/null";
        if (system(losetup.c_str()) != 0) {
            Logs::warning("Failed to setup loop device, using direct dd method");
            return false;
        }
        
        std::string tmpMount = "/tmp/myiso_iso_" + std::to_string(getpid());
        std::filesystem::create_directories(tmpMount);
        
        if (mount(loopDevice.c_str(), tmpMount.c_str(), "iso9660", 
                  MS_RDONLY, nullptr) != 0) {
            system(("losetup -d " + loopDevice + " 2>/dev/null").c_str());
            std::filesystem::remove_all(tmpMount);
            return false;
        }
        
        std::string cpCmd = "cp -r " + tmpMount + "/* " + destination + "/ 2>/dev/null";
        system(cpCmd.c_str());
        
        umount(tmpMount.c_str());
        system(("losetup -d " + loopDevice + " 2>/dev/null").c_str());
        std::filesystem::remove_all(tmpMount);
        
        Logs::success("ISO contents extracted");
        return true;
    }
    
    bool BootloaderInstaller::installSyslinux() {
        Logs::info("Installing SYSLINUX bootloader");
        
        std::filesystem::create_directories(mountPoint);
        
        std::string part1 = device;
        if (device.back() >= '0' && device.back() <= '9') {
            part1 = device + "1";
        } else {
            part1 = device + "1";
        }
        
        if (mount(part1.c_str(), mountPoint.c_str(), "vfat", 0, nullptr) != 0) {
            Logs::warning("Failed to mount partition for bootloader installation");
            return false;
        }
        
        std::string syslinuxDir = mountPoint + "/syslinux";
        std::filesystem::create_directories(syslinuxDir);
        
        std::ofstream cfg(syslinuxDir + "/syslinux.cfg");
        if (cfg.is_open()) {
            cfg << "DEFAULT menu.c32\n";
            cfg << "PROMPT 0\n";
            cfg << "TIMEOUT 300\n";
            cfg << "\n";
            cfg << "MENU TITLE MyISO Boot Menu\n";
            cfg << "MENU BACKGROUND splash.png\n";
            cfg << "\n";
            cfg << "LABEL linux\n";
            cfg << "  MENU LABEL Boot Linux\n";
            cfg << "  KERNEL /casper/vmlinuz\n";
            cfg << "  APPEND initrd=/casper/initrd boot=casper quiet splash ---\n";
            cfg << "\n";
            cfg << "LABEL persistent\n";
            cfg << "  MENU LABEL Boot with Persistence\n";
            cfg << "  KERNEL /casper/vmlinuz\n";
            cfg << "  APPEND initrd=/casper/initrd boot=casper persistent quiet splash ---\n";
            cfg.close();
        }
        
        if (!writeSyslinuxMBR()) {
            Logs::warning("Failed to write SYSLINUX MBR");
        }
        
        umount(mountPoint.c_str());
        
        Logs::success("SYSLINUX bootloader installed");
        return true;
    }
    
    bool BootloaderInstaller::installGrub() {
        Logs::info("Installing GRUB bootloader");
        
        std::filesystem::create_directories(mountPoint);
        
        std::string part1 = device + "1";
        if (mount(part1.c_str(), mountPoint.c_str(), "vfat", 0, nullptr) != 0) {
            return false;
        }
        
        std::string grubDir = mountPoint + "/boot/grub";
        std::filesystem::create_directories(grubDir);
        
        std::ofstream cfg(grubDir + "/grub.cfg");
        if (cfg.is_open()) {
            cfg << "set timeout=10\n";
            cfg << "set default=0\n";
            cfg << "\n";
            cfg << "menuentry \"Boot Linux\" {\n";
            cfg << "  linux /casper/vmlinuz boot=casper quiet splash ---\n";
            cfg << "  initrd /casper/initrd\n";
            cfg << "}\n";
            cfg << "\n";
            cfg << "menuentry \"Boot with Persistence\" {\n";
            cfg << "  linux /casper/vmlinuz boot=casper persistent quiet splash ---\n";
            cfg << "  initrd /casper/initrd\n";
            cfg << "}\n";
            cfg.close();
        }
        
        umount(mountPoint.c_str());
        
        Logs::success("GRUB bootloader installed");
        return true;
    }
    
    bool BootloaderInstaller::writeSyslinuxMBR() {
        std::vector<uint8_t> mbrCode = getSyslinuxMBRCode();
        
        int fd = open(device.c_str(), O_RDWR | O_SYNC);
        if (fd < 0) return false;
        
        if (write(fd, mbrCode.data(), 440) != 440) {
            close(fd);
            return false;
        }
        
        fsync(fd);
        close(fd);
        
        return true;
    }
    
    std::vector<uint8_t> BootloaderInstaller::getSyslinuxMBRCode() {
        std::vector<uint8_t> mbr(440, 0);
        
        uint8_t bootCode[] = {
            0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x7C,
            0xFB, 0xFC, 0xBF, 0x00, 0x06, 0xB9, 0x00, 0x01, 0xF3, 0xA5, 0xEA, 0x1F,
            0x06, 0x00, 0x00, 0xB4, 0x41, 0xBB, 0xAA, 0x55, 0xCD, 0x13, 0x72, 0x3E,
            0x81, 0xFB, 0x55, 0xAA, 0x75, 0x38, 0x83, 0xE1, 0x01, 0x74, 0x33, 0x66,
            0xA1, 0x10, 0x7C, 0x66, 0x3B, 0x46, 0xF8, 0x0F, 0x82, 0x2A, 0x00
        };
        
        size_t codeSize = sizeof(bootCode);
        if (codeSize > 440) codeSize = 440;
        
        memcpy(mbr.data(), bootCode, codeSize);
        
        return mbr;
    }
    
    bool BootloaderInstaller::makeBootable() {
        Logs::info("Making device bootable");
        
        switch (bootType) {
            case BootType::SYSLINUX:
            case BootType::ISOLINUX:
                return installSyslinux();
            case BootType::GRUB:
                return installGrub();
            default:
                return installSyslinux();
        }
    }
    
    bool installBootloader(const std::string& device, const std::string& isoPath) {
        BootloaderInstaller installer(device);
        
        installer.detectBootType(isoPath);
        
        return installer.makeBootable();
    }
}
