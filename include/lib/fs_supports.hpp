#ifndef FS_SUPPORTS_HPP
#define FS_SUPPORTS_HPP

#include <string>
#include <vector>

namespace FilesystemSupport {
    enum class FSType {
        EXT4,
        NTFS,
        EXFAT,
        FAT32,
        FAT64,
        UNKNOWN
    };
    
    FSType parseFSType(const std::string& fsName);
    bool isSupported(FSType fs);
    std::string getFSName(FSType fs);
    std::vector<std::string> getSupportedFilesystems();
    bool formatPartition(const std::string& device, FSType fs);
}

#endif // FS_SUPPORTS_HPP
