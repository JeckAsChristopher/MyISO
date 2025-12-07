#ifndef ISO_ANALYZER_HPP
#define ISO_ANALYZER_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace ISOAnalyzer {
    
    struct PartitionInfo {
        uint64_t startLBA;
        uint64_t sectorCount;
        uint8_t type;
        bool bootable;
        std::string label;
        std::string filesystem;
    };
    
    struct ISOStructure {
        bool isHybrid;
        bool hasElTorito;
        bool hasUEFI;
        bool hasLegacyBoot;
        bool isMultiBoot;
        int requiredPartitions;
        uint64_t isoDataSize;
        uint64_t bootSectorLocation;
        std::vector<PartitionInfo> embeddedPartitions;
        std::string bootType;
        std::vector<std::string> bootFiles;
    };
    
    class SmartAnalyzer {
    public:
        static ISOStructure analyzeISO(const std::string& isoPath);
        static int calculateRequiredPartitions(const ISOStructure& structure, 
                                               bool withPersistence);
        static std::string getRecommendedStrategy(const ISOStructure& structure);
        
    private:
        static bool checkElTorito(const std::string& isoPath);
        static bool checkUEFI(const std::string& isoPath);
        static bool checkHybridISO(const std::string& isoPath);
        static std::vector<PartitionInfo> extractEmbeddedPartitions(const std::string& isoPath);
        static std::vector<std::string> findBootFiles(const std::string& isoPath);
    };
    
    enum class BurnStrategy {
        RAW_COPY,           // Direct dd-style copy
        SMART_EXTRACT,      // Extract and reorganize
        HYBRID_PRESERVE,    // Preserve hybrid structure
        MULTIPART           // Multiple partition setup
    };
    
    BurnStrategy determineBurnStrategy(const ISOStructure& structure);
}

#endif // ISO_ANALYZER_HPP
