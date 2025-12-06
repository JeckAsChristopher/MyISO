#include "lib/fs_supports.hpp"
#include "lib/fs_creator.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <algorithm>

namespace FilesystemSupport {
    
    FSType parseFSType(const std::string& fsName) {
        std::string lower = fsName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower == "ext4") return FSType::EXT4;
        if (lower == "ntfs") return FSType::NTFS;
        if (lower == "exfat") return FSType::EXFAT;
        if (lower == "fat32") return FSType::FAT32;
        if (lower == "fat64") return FSType::FAT64;
        
        return FSType::UNKNOWN;
    }
    
    bool isSupported(FSType fs) {
        return fs != FSType::UNKNOWN;
    }
    
    std::string getFSName(FSType fs) {
        switch (fs) {
            case FSType::EXT4: return "ext4";
            case FSType::NTFS: return "ntfs";
            case FSType::EXFAT: return "exfat";
            case FSType::FAT32: return "fat32";
            case FSType::FAT64: return "fat64";
            default: return "unknown";
        }
    }
    
    std::vector<std::string> getSupportedFilesystems() {
        return {"ext4", "ntfs", "exfat", "FAT32", "FAT64"};
    }
    
    bool formatPartition(const std::string& device, FSType fs, const std::string& label) {
        Logs::info("Formatting " + device + " as " + getFSName(fs));
        
        try {
            std::string fsName = getFSName(fs);
            FilesystemCreator::createFilesystem(device, fsName, label);
            
            Logs::success("Formatted " + device + " as " + getFSName(fs));
            return true;
            
        } catch (const std::exception& e) {
            throw FilesystemError("Failed to format partition: " + std::string(e.what()));
        }
    }
}
