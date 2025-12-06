#ifndef BOOTLOADER_HPP
#define BOOTLOADER_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace Bootloader {
    
    enum class BootType {
        SYSLINUX,
        GRUB,
        ISOLINUX,
        AUTO
    };
    
    class BootloaderInstaller {
    private:
        std::string device;
        std::string mountPoint;
        BootType bootType;
        
    public:
        BootloaderInstaller(const std::string& dev, BootType type = BootType::AUTO);
        ~BootloaderInstaller();
        
        bool detectBootType(const std::string& isoPath);
        bool installSyslinux();
        bool installGrub();
        bool copyBootFiles(const std::string& isoPath);
        bool makeBootable();
        bool extractISO(const std::string& isoPath, const std::string& destination);
        
    private:
        bool installMBR();
        bool writeSyslinuxMBR();
        std::vector<uint8_t> getSyslinuxMBRCode();
    };
    
    bool installBootloader(const std::string& device, const std::string& isoPath);
}

#endif // BOOTLOADER_HPP
