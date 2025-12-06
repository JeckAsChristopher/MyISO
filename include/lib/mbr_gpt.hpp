#ifndef MBR_GPT_HPP
#define MBR_GPT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace BootStructures {
    
    enum class PartitionType : uint8_t {
        EMPTY = 0x00,
        FAT32_LBA = 0x0C,
        LINUX_NATIVE = 0x83,
        LINUX_EXTENDED = 0x85,
        NTFS = 0x07,
        EXFAT = 0x07,
        EFI_SYSTEM = 0xEF
    };
    
    enum class TableType {
        MBR,
        GPT,
        HYBRID
    };
    
    #pragma pack(push, 1)
    struct MBRPartitionEntry {
        uint8_t status;
        uint8_t firstCHS[3];
        uint8_t partitionType;
        uint8_t lastCHS[3];
        uint32_t firstLBA;
        uint32_t sectorCount;
    };
    
    struct MBR {
        uint8_t bootCode[440];
        uint32_t diskSignature;
        uint16_t reserved;
        MBRPartitionEntry partitions[4];
        uint16_t signature;
    };
    
    struct GPTHeader {
        char signature[8];
        uint32_t revision;
        uint32_t headerSize;
        uint32_t headerCRC32;
        uint32_t reserved;
        uint64_t currentLBA;
        uint64_t backupLBA;
        uint64_t firstUsableLBA;
        uint64_t lastUsableLBA;
        uint8_t diskGUID[16];
        uint64_t partitionEntryLBA;
        uint32_t numberOfPartitionEntries;
        uint32_t sizeOfPartitionEntry;
        uint32_t partitionArrayCRC32;
    };
    
    struct GPTPartitionEntry {
        uint8_t partitionTypeGUID[16];
        uint8_t uniquePartitionGUID[16];
        uint64_t firstLBA;
        uint64_t lastLBA;
        uint64_t attributes;
        uint16_t partitionName[36];
    };
    #pragma pack(pop)
    
    class PartitionTable {
    private:
        std::string device;
        int deviceFd;
        uint64_t deviceSectors;
        TableType tableType;
        
    public:
        PartitionTable(const std::string& dev, TableType type = TableType::MBR);
        ~PartitionTable();
        
        bool initialize();
        bool createMBR();
        bool createGPT();
        bool addMBRPartition(uint32_t startLBA, uint32_t sectorCount, 
                            PartitionType type, bool bootable = false);
        bool addGPTPartition(uint64_t startLBA, uint64_t sectorCount,
                            const uint8_t* typeGUID, const std::string& name);
        bool makeBootable();
        bool writeBootloader(const std::vector<uint8_t>& bootCode);
        bool commit();
        
    private:
        void calculateCHS(uint32_t lba, uint8_t* chs);
        uint32_t calculateCRC32(const void* data, size_t length);
        void generateGUID(uint8_t* guid);
    };
}

#endif // MBR_GPT_HPP
