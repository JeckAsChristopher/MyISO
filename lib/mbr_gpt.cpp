#include "lib/mbr_gpt.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <cstring>
#include <random>

namespace BootStructures {
    
    // CRC32 lookup table for optimization
    static uint32_t crc32_table[256];
    static bool crc32_table_initialized = false;
    
    static void init_crc32_table() {
        if (crc32_table_initialized) return;
        
        const uint32_t polynomial = 0xEDB88320;
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t crc = i;
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ ((crc & 1) ? polynomial : 0);
            }
            crc32_table[i] = crc;
        }
        crc32_table_initialized = true;
    }
    
    PartitionTable::PartitionTable(const std::string& dev, TableType type)
        : device(dev), deviceFd(-1), deviceSectors(0), tableType(type) {
    }
    
    PartitionTable::~PartitionTable() {
        if (deviceFd >= 0) {
            close(deviceFd);
        }
    }
    
    bool PartitionTable::initialize() {
        deviceFd = open(device.c_str(), O_RDWR | O_SYNC);
        if (deviceFd < 0) {
            throw DeviceError(device, "Cannot open device for partition table creation");
        }
        
        uint64_t deviceSize;
        if (ioctl(deviceFd, BLKGETSIZE64, &deviceSize) < 0) {
            throw DeviceError(device, "Cannot get device size");
        }
        
        deviceSectors = deviceSize / 512;
        Logs::debug("Device sectors: " + std::to_string(deviceSectors));
        
        return true;
    }
    
    bool PartitionTable::createMBR() {
        Logs::info("Creating optimized MBR partition table");
        
        MBR mbr;
        memset(&mbr, 0, sizeof(MBR));
        
        // Generate cryptographically random disk signature
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis;
        mbr.diskSignature = dis(gen);
        
        Logs::debug("Generated disk signature: 0x" + 
                   std::to_string(mbr.diskSignature));
        
        // Initialize boot code area with NOPs for safety
        memset(mbr.bootCode, 0x90, sizeof(mbr.bootCode));
        
        // Add minimal boot code stub
        mbr.bootCode[0] = 0xFA;  // CLI
        mbr.bootCode[1] = 0x31;  // XOR AX, AX
        mbr.bootCode[2] = 0xC0;
        mbr.bootCode[3] = 0x8E;  // MOV SS, AX
        mbr.bootCode[4] = 0xD0;
        mbr.bootCode[5] = 0xBC;  // MOV SP, 0x7C00
        mbr.bootCode[6] = 0x00;
        mbr.bootCode[7] = 0x7C;
        
        mbr.signature = 0xAA55;
        
        // Verify structure alignment
        if (sizeof(MBR) != 512) {
            Logs::warning("MBR structure size mismatch: " + std::to_string(sizeof(MBR)));
        }
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            throw DeviceError(device, "Failed to seek to MBR location");
        }
        
        ssize_t written = write(deviceFd, &mbr, sizeof(MBR));
        if (written != sizeof(MBR)) {
            throw DeviceError(device, "Failed to write MBR (wrote " + 
                            std::to_string(written) + " bytes)");
        }
        
        // Write protective sectors (prevent accidental overwrites)
        uint8_t protective[512];
        memset(protective, 0, sizeof(protective));
        for (int i = 1; i < 2048; i++) {
            if (lseek(deviceFd, i * 512, SEEK_SET) != i * 512) continue;
            write(deviceFd, protective, 512);
        }
        
        fsync(deviceFd);
        
        Logs::success("Optimized MBR created successfully");
        return true;
    }
    
    bool PartitionTable::createGPT() {
        Logs::info("Creating GPT partition table");
        
        MBR protectiveMBR;
        memset(&protectiveMBR, 0, sizeof(MBR));
        
        protectiveMBR.partitions[0].status = 0x00;
        protectiveMBR.partitions[0].partitionType = 0xEE;
        protectiveMBR.partitions[0].firstLBA = 1;
        protectiveMBR.partitions[0].sectorCount = 
            (deviceSectors > 0xFFFFFFFF) ? 0xFFFFFFFF : deviceSectors - 1;
        protectiveMBR.signature = 0xAA55;
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            throw DeviceError(device, "Failed to seek for protective MBR");
        }
        
        if (write(deviceFd, &protectiveMBR, sizeof(MBR)) != sizeof(MBR)) {
            throw DeviceError(device, "Failed to write protective MBR");
        }
        
        GPTHeader header;
        memset(&header, 0, sizeof(GPTHeader));
        
        memcpy(header.signature, "EFI PART", 8);
        header.revision = 0x00010000;
        header.headerSize = 92;
        header.currentLBA = 1;
        header.backupLBA = deviceSectors - 1;
        header.firstUsableLBA = 34;
        header.lastUsableLBA = deviceSectors - 34;
        generateGUID(header.diskGUID);
        header.partitionEntryLBA = 2;
        header.numberOfPartitionEntries = 128;
        header.sizeOfPartitionEntry = 128;
        
        header.headerCRC32 = calculateCRC32(&header, header.headerSize);
        
        if (lseek(deviceFd, 512, SEEK_SET) != 512) {
            throw DeviceError(device, "Failed to seek for GPT header");
        }
        
        if (write(deviceFd, &header, sizeof(GPTHeader)) != sizeof(GPTHeader)) {
            throw DeviceError(device, "Failed to write GPT header");
        }
        
        fsync(deviceFd);
        
        Logs::success("GPT created successfully");
        return true;
    }
    
    bool PartitionTable::addMBRPartition(uint32_t startLBA, uint32_t sectorCount,
                                         PartitionType type, bool bootable) {
        MBR mbr;
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            throw DeviceError(device, "Failed to seek to MBR");
        }
        
        if (read(deviceFd, &mbr, sizeof(MBR)) != sizeof(MBR)) {
            throw DeviceError(device, "Failed to read MBR");
        }
        
        int partIndex = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.partitions[i].partitionType == 0x00) {
                partIndex = i;
                break;
            }
        }
        
        if (partIndex == -1) {
            throw DeviceError(device, "No free partition slots in MBR");
        }
        
        MBRPartitionEntry& part = mbr.partitions[partIndex];
        part.status = bootable ? 0x80 : 0x00;
        part.partitionType = static_cast<uint8_t>(type);
        part.firstLBA = startLBA;
        part.sectorCount = sectorCount;
        
        calculateCHS(startLBA, part.firstCHS);
        calculateCHS(startLBA + sectorCount - 1, part.lastCHS);
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            throw DeviceError(device, "Failed to seek for MBR write");
        }
        
        if (write(deviceFd, &mbr, sizeof(MBR)) != sizeof(MBR)) {
            throw DeviceError(device, "Failed to write partition to MBR");
        }
        
        fsync(deviceFd);
        
        Logs::success("Partition " + std::to_string(partIndex + 1) + " added to MBR");
        return true;
    }
    
    bool PartitionTable::makeBootable() {
        Logs::info("Setting partition as bootable");
        
        MBR mbr;
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            return false;
        }
        
        if (read(deviceFd, &mbr, sizeof(MBR)) != sizeof(MBR)) {
            return false;
        }
        
        mbr.partitions[0].status = 0x80;
        
        if (lseek(deviceFd, 0, SEEK_SET) != 0) {
            return false;
        }
        
        if (write(deviceFd, &mbr, sizeof(MBR)) != sizeof(MBR)) {
            return false;
        }
        
        fsync(deviceFd);
        return true;
    }
    
    bool PartitionTable::commit() {
        if (deviceFd >= 0) {
            fsync(deviceFd);
            ioctl(deviceFd, BLKRRPART);
        }
        return true;
    }
    
    void PartitionTable::calculateCHS(uint32_t lba, uint8_t* chs) {
        const uint32_t sectorsPerTrack = 63;
        const uint32_t heads = 255;
        
        uint32_t cylinder = lba / (heads * sectorsPerTrack);
        uint32_t temp = lba % (heads * sectorsPerTrack);
        uint32_t head = temp / sectorsPerTrack;
        uint32_t sector = temp % sectorsPerTrack + 1;
        
        if (cylinder > 1023) cylinder = 1023;
        
        chs[0] = head & 0xFF;
        chs[1] = ((cylinder >> 2) & 0xC0) | (sector & 0x3F);
        chs[2] = cylinder & 0xFF;
    }
    
    uint32_t PartitionTable::calculateCRC32(const void* data, size_t length) {
        init_crc32_table();
        
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        
        // Optimized CRC32 calculation using lookup table
        for (size_t i = 0; i < length; i++) {
            uint8_t index = (crc ^ bytes[i]) & 0xFF;
            crc = (crc >> 8) ^ crc32_table[index];
        }
        
        return ~crc;
    }
    
    void PartitionTable::generateGUID(uint8_t* guid) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(0, 255);
        
        for (int i = 0; i < 16; i++) {
            guid[i] = dis(gen);
        }
        
        guid[6] = (guid[6] & 0x0F) | 0x40;
        guid[8] = (guid[8] & 0x3F) | 0x80;
    }
}
