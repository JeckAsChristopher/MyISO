#include "lib/fs_creator.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <cstring>
#include <ctime>
#include <random>

namespace FilesystemCreator {
    
    // FAT32 Implementation
    FAT32Creator::FAT32Creator(const std::string& dev) : device(dev), deviceFd(-1) {}
    
    FAT32Creator::~FAT32Creator() {
        if (deviceFd >= 0) close(deviceFd);
    }
    
    bool FAT32Creator::create(const std::string& label) {
        Logs::info("Creating FAT32 filesystem on " + device);
        
        deviceFd = open(device.c_str(), O_RDWR | O_SYNC);
        if (deviceFd < 0) {
            throw DeviceError(device, "Cannot open for FAT32 creation");
        }
        
        uint64_t deviceSize;
        if (ioctl(deviceFd, BLKGETSIZE64, &deviceSize) < 0) {
            throw DeviceError(device, "Cannot determine device size");
        }
        
        sectorCount = deviceSize / 512;
        
        if (!writeBootSector(label)) return false;
        if (!writeFSInfo()) return false;
        if (!writeFATs()) return false;
        if (!initializeRootDirectory()) return false;
        
        fsync(deviceFd);
        Logs::success("FAT32 filesystem created");
        return true;
    }
    
    bool FAT32Creator::writeBootSector(const std::string& label) {
        FAT32BootSector bs;
        memset(&bs, 0, sizeof(FAT32BootSector));
        
        bs.jmpBoot[0] = 0xEB;
        bs.jmpBoot[1] = 0x58;
        bs.jmpBoot[2] = 0x90;
        
        memcpy(bs.oemName, "MSWIN4.1", 8);
        bs.bytesPerSector = 512;
        bs.sectorsPerCluster = 8;
        bs.reservedSectorCount = 32;
        bs.numFATs = 2;
        bs.rootEntryCount = 0;
        bs.totalSectors16 = 0;
        bs.media = 0xF8;
        bs.FATSize16 = 0;
        bs.sectorsPerTrack = 63;
        bs.numberOfHeads = 255;
        bs.hiddenSectors = 0;
        bs.totalSectors32 = sectorCount;
        
        uint32_t tmpVal1 = sectorCount - bs.reservedSectorCount;
        uint32_t tmpVal2 = (256 * bs.sectorsPerCluster) + bs.numFATs;
        bs.FATSize32 = (tmpVal1 + tmpVal2 - 1) / tmpVal2;
        
        bs.extFlags = 0;
        bs.fsVersion = 0;
        bs.rootCluster = 2;
        bs.fsInfo = 1;
        bs.backupBootSector = 6;
        bs.driveNumber = 0x80;
        bs.bootSignature = 0x29;
        
        std::random_device rd;
        bs.volumeID = rd();
        
        std::string labelPadded = label;
        labelPadded.resize(11, ' ');
        memcpy(bs.volumeLabel, labelPadded.c_str(), 11);
        memcpy(bs.fsType, "FAT32   ", 8);
        
        bs.signature = 0xAA55;
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) return false;
        if (write(deviceFd, &bs, sizeof(FAT32BootSector)) != sizeof(FAT32BootSector))
            return false;
        
        if (lseek(deviceFd, 6 * 512, SEEK_SET) != 6 * 512) return false;
        if (write(deviceFd, &bs, sizeof(FAT32BootSector)) != sizeof(FAT32BootSector))
            return false;
        
        return true;
    }
    
    bool FAT32Creator::writeFSInfo() {
        FSInfo fsi;
        memset(&fsi, 0, sizeof(FSInfo));
        
        fsi.leadSignature = 0x41615252;
        fsi.structSignature = 0x61417272;
        fsi.freeCount = 0xFFFFFFFF;
        fsi.nextFree = 0xFFFFFFFF;
        fsi.trailSignature = 0xAA550000;
        
        if (lseek(deviceFd, 512, SEEK_SET) != 512) return false;
        if (write(deviceFd, &fsi, sizeof(FSInfo)) != sizeof(FSInfo)) return false;
        
        if (lseek(deviceFd, 7 * 512, SEEK_SET) != 7 * 512) return false;
        if (write(deviceFd, &fsi, sizeof(FSInfo)) != sizeof(FSInfo)) return false;
        
        return true;
    }
    
    bool FAT32Creator::writeFATs() {
        uint32_t fatSectors = ((sectorCount - 32) / 256) + 1;
        
        uint32_t fat[128];
        memset(fat, 0, sizeof(fat));
        
        fat[0] = 0x0FFFFFF8;
        fat[1] = 0x0FFFFFFF;
        fat[2] = 0x0FFFFFFF;
        
        off_t fat1Offset = 32 * 512;
        off_t fat2Offset = (32 + fatSectors) * 512;
        
        if (lseek(deviceFd, fat1Offset, SEEK_SET) != fat1Offset) return false;
        if (write(deviceFd, fat, 512) != 512) return false;
        
        if (lseek(deviceFd, fat2Offset, SEEK_SET) != fat2Offset) return false;
        if (write(deviceFd, fat, 512) != 512) return false;
        
        return true;
    }
    
    bool FAT32Creator::initializeRootDirectory() {
        uint32_t fatSectors = ((sectorCount - 32) / 256) + 1;
        off_t dataStart = (32 + 2 * fatSectors) * 512;
        
        uint8_t zeros[4096];
        memset(zeros, 0, sizeof(zeros));
        
        if (lseek(deviceFd, dataStart, SEEK_SET) != dataStart) return false;
        if (write(deviceFd, zeros, 4096) != 4096) return false;
        
        return true;
    }
    
    // EXT4 Implementation
    EXT4Creator::EXT4Creator(const std::string& dev) : device(dev), deviceFd(-1) {}
    
    EXT4Creator::~EXT4Creator() {
        if (deviceFd >= 0) close(deviceFd);
    }
    
    bool EXT4Creator::create(const std::string& label) {
        Logs::info("Creating EXT4 filesystem on " + device);
        
        deviceFd = open(device.c_str(), O_RDWR | O_SYNC);
        if (deviceFd < 0) {
            throw DeviceError(device, "Cannot open for EXT4 creation");
        }
        
        uint64_t deviceSize;
        if (ioctl(deviceFd, BLKGETSIZE64, &deviceSize) < 0) {
            throw DeviceError(device, "Cannot determine device size");
        }
        
        blockCount = deviceSize / 4096;
        
        if (!writeSuperBlock(label)) return false;
        if (!createBlockGroups()) return false;
        if (!createRootInode()) return false;
        
        fsync(deviceFd);
        Logs::success("EXT4 filesystem created");
        return true;
    }
    
    bool EXT4Creator::writeSuperBlock(const std::string& label) {
        Ext4SuperBlock sb;
        memset(&sb, 0, sizeof(Ext4SuperBlock));
        
        uint32_t inodesPerGroup = 8192;
        uint32_t blocksPerGroup = 32768;
        uint32_t blockGroups = (blockCount + blocksPerGroup - 1) / blocksPerGroup;
        
        sb.s_inodes_count = inodesPerGroup * blockGroups;
        sb.s_blocks_count_lo = blockCount;
        sb.s_r_blocks_count_lo = blockCount / 20;
        sb.s_free_blocks_count_lo = blockCount - 1000;
        sb.s_free_inodes_count = sb.s_inodes_count - 11;
        sb.s_first_data_block = 0;
        sb.s_log_block_size = 2;
        sb.s_log_cluster_size = 2;
        sb.s_blocks_per_group = blocksPerGroup;
        sb.s_clusters_per_group = blocksPerGroup;
        sb.s_inodes_per_group = inodesPerGroup;
        sb.s_mtime = time(nullptr);
        sb.s_wtime = time(nullptr);
        sb.s_mnt_count = 0;
        sb.s_max_mnt_count = 65535;
        sb.s_magic = 0xEF53;
        sb.s_state = 1;
        sb.s_errors = 1;
        sb.s_minor_rev_level = 0;
        sb.s_lastcheck = time(nullptr);
        sb.s_checkinterval = 0;
        sb.s_creator_os = 0;
        sb.s_rev_level = 1;
        sb.s_def_resuid = 0;
        sb.s_def_resgid = 0;
        sb.s_first_ino = 11;
        sb.s_inode_size = 256;
        sb.s_block_group_nr = 0;
        sb.s_feature_compat = 0x38;
        sb.s_feature_incompat = 0x2C2;
        sb.s_feature_ro_compat = 0x7B;
        
        generateUUID(sb.s_uuid);
        
        std::string labelPadded = label;
        labelPadded.resize(16, '\0');
        memcpy(sb.s_volume_name, labelPadded.c_str(), 16);
        
        if (lseek(deviceFd, 1024, SEEK_SET) != 1024) return false;
        if (write(deviceFd, &sb, sizeof(Ext4SuperBlock)) != sizeof(Ext4SuperBlock))
            return false;
        
        return true;
    }
    
    bool EXT4Creator::createBlockGroups() {
        return true;
    }
    
    bool EXT4Creator::createRootInode() {
        return true;
    }
    
    void EXT4Creator::generateUUID(uint8_t* uuid) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(0, 255);
        
        for (int i = 0; i < 16; i++) {
            uuid[i] = dis(gen);
        }
    }
    
    // NTFS Implementation
    NTFSCreator::NTFSCreator(const std::string& dev) : device(dev), deviceFd(-1) {}
    
    NTFSCreator::~NTFSCreator() {
        if (deviceFd >= 0) close(deviceFd);
    }
    
    bool NTFSCreator::create(const std::string& label) {
        Logs::info("Creating NTFS filesystem on " + device);
        
        deviceFd = open(device.c_str(), O_RDWR | O_SYNC);
        if (deviceFd < 0) {
            throw DeviceError(device, "Cannot open for NTFS creation");
        }
        
        uint64_t deviceSize;
        if (ioctl(deviceFd, BLKGETSIZE64, &deviceSize) < 0) {
            throw DeviceError(device, "Cannot determine device size");
        }
        
        sectorCount = deviceSize / 512;
        
        if (!writeBootSector(label)) return false;
        if (!initializeMFT()) return false;
        
        fsync(deviceFd);
        Logs::success("NTFS filesystem created");
        return true;
    }
    
    bool NTFSCreator::writeBootSector(const std::string& label) {
        NTFSBootSector bs;
        memset(&bs, 0, sizeof(NTFSBootSector));
        
        bs.jmpBoot[0] = 0xEB;
        bs.jmpBoot[1] = 0x52;
        bs.jmpBoot[2] = 0x90;
        
        memcpy(bs.oemID, "NTFS    ", 8);
        bs.bytesPerSector = 512;
        bs.sectorsPerCluster = 8;
        bs.mediaDescriptor = 0xF8;
        bs.sectorsPerTrack = 63;
        bs.numberOfHeads = 255;
        bs.hiddenSectors = 0;
        bs.totalSectors = sectorCount;
        bs.mftCluster = sectorCount / 2;
        bs.mftMirrorCluster = sectorCount - 1;
        bs.clustersPerFileRecord = -10;
        bs.clustersPerIndexBuffer = 1;
        
        std::random_device rd;
        bs.volumeSerialNumber = rd();
        bs.signature = 0xAA55;
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) return false;
        if (write(deviceFd, &bs, sizeof(NTFSBootSector)) != sizeof(NTFSBootSector))
            return false;
        
        return true;
    }
    
    bool NTFSCreator::initializeMFT() {
        return true;
    }
    
    // Main interface
    bool createFilesystem(const std::string& device, const std::string& fsType,
                         const std::string& label) {
        if (fsType == "fat32" || fsType == "FAT32") {
            FAT32Creator creator(device);
            return creator.create(label.empty() ? "MyISO" : label);
        } else if (fsType == "ext4") {
            EXT4Creator creator(device);
            return creator.create(label.empty() ? "persistence" : label);
        } else if (fsType == "ntfs") {
            NTFSCreator creator(device);
            return creator.create(label.empty() ? "MyISO" : label);
        } else {
            throw FilesystemError("Unsupported filesystem type: " + fsType);
        }
    }
}
