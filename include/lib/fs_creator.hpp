#ifndef FS_CREATOR_HPP
#define FS_CREATOR_HPP

#include <string>
#include <cstdint>

namespace FilesystemCreator {
    
    class FAT32Creator {
    private:
        std::string device;
        int deviceFd;
        uint64_t sectorCount;
        
        #pragma pack(push, 1)
        struct FAT32BootSector {
            uint8_t jmpBoot[3];
            char oemName[8];
            uint16_t bytesPerSector;
            uint8_t sectorsPerCluster;
            uint16_t reservedSectorCount;
            uint8_t numFATs;
            uint16_t rootEntryCount;
            uint16_t totalSectors16;
            uint8_t media;
            uint16_t FATSize16;
            uint16_t sectorsPerTrack;
            uint16_t numberOfHeads;
            uint32_t hiddenSectors;
            uint32_t totalSectors32;
            uint32_t FATSize32;
            uint16_t extFlags;
            uint16_t fsVersion;
            uint32_t rootCluster;
            uint16_t fsInfo;
            uint16_t backupBootSector;
            uint8_t reserved[12];
            uint8_t driveNumber;
            uint8_t reserved1;
            uint8_t bootSignature;
            uint32_t volumeID;
            char volumeLabel[11];
            char fsType[8];
            uint8_t bootCode[420];
            uint16_t signature;
        };
        
        struct FSInfo {
            uint32_t leadSignature;
            uint8_t reserved1[480];
            uint32_t structSignature;
            uint32_t freeCount;
            uint32_t nextFree;
            uint8_t reserved2[12];
            uint32_t trailSignature;
        };
        #pragma pack(pop)
        
    public:
        explicit FAT32Creator(const std::string& dev);
        ~FAT32Creator();
        
        bool create(const std::string& label = "MyISO");
        
    private:
        bool writeBootSector(const std::string& label);
        bool writeFSInfo();
        bool writeFATs();
        bool initializeRootDirectory();
    };
    
    class EXT4Creator {
    private:
        std::string device;
        int deviceFd;
        uint64_t blockCount;
        
        #pragma pack(push, 1)
        struct Ext4SuperBlock {
            uint32_t s_inodes_count;
            uint32_t s_blocks_count_lo;
            uint32_t s_r_blocks_count_lo;
            uint32_t s_free_blocks_count_lo;
            uint32_t s_free_inodes_count;
            uint32_t s_first_data_block;
            uint32_t s_log_block_size;
            uint32_t s_log_cluster_size;
            uint32_t s_blocks_per_group;
            uint32_t s_clusters_per_group;
            uint32_t s_inodes_per_group;
            uint32_t s_mtime;
            uint32_t s_wtime;
            uint16_t s_mnt_count;
            uint16_t s_max_mnt_count;
            uint16_t s_magic;
            uint16_t s_state;
            uint16_t s_errors;
            uint16_t s_minor_rev_level;
            uint32_t s_lastcheck;
            uint32_t s_checkinterval;
            uint32_t s_creator_os;
            uint32_t s_rev_level;
            uint16_t s_def_resuid;
            uint16_t s_def_resgid;
            uint32_t s_first_ino;
            uint16_t s_inode_size;
            uint16_t s_block_group_nr;
            uint32_t s_feature_compat;
            uint32_t s_feature_incompat;
            uint32_t s_feature_ro_compat;
            uint8_t s_uuid[16];
            char s_volume_name[16];
            uint8_t padding[924];
        };
        #pragma pack(pop)
        
    public:
        explicit EXT4Creator(const std::string& dev);
        ~EXT4Creator();
        
        bool create(const std::string& label = "persistence");
        
    private:
        bool writeSuperBlock(const std::string& label);
        bool createBlockGroups();
        bool createRootInode();
        void generateUUID(uint8_t* uuid);
    };
    
    class NTFSCreator {
    private:
        std::string device;
        int deviceFd;
        uint64_t sectorCount;
        
        #pragma pack(push, 1)
        struct NTFSBootSector {
            uint8_t jmpBoot[3];
            char oemID[8];
            uint16_t bytesPerSector;
            uint8_t sectorsPerCluster;
            uint16_t reserved1;
            uint8_t reserved2;
            uint16_t reserved3;
            uint16_t reserved4;
            uint8_t mediaDescriptor;
            uint16_t reserved5;
            uint16_t sectorsPerTrack;
            uint16_t numberOfHeads;
            uint32_t hiddenSectors;
            uint32_t reserved6;
            uint32_t reserved7;
            uint64_t totalSectors;
            uint64_t mftCluster;
            uint64_t mftMirrorCluster;
            int8_t clustersPerFileRecord;
            uint8_t reserved8[3];
            int8_t clustersPerIndexBuffer;
            uint8_t reserved9[3];
            uint64_t volumeSerialNumber;
            uint32_t checksum;
            uint8_t bootCode[426];
            uint16_t signature;
        };
        #pragma pack(pop)
        
    public:
        explicit NTFSCreator(const std::string& dev);
        ~NTFSCreator();
        
        bool create(const std::string& label = "MyISO");
        
    private:
        bool writeBootSector(const std::string& label);
        bool initializeMFT();
    };
    
    bool createFilesystem(const std::string& device, const std::string& fsType, 
                         const std::string& label = "");
}

#endif // FS_CREATOR_HPP
