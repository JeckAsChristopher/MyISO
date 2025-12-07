#ifndef SMART_BURNER_HPP
#define SMART_BURNER_HPP

#include "iso_analyzer.hpp"
#include <string>
#include <vector>

namespace SmartBurner {
    
    struct BurnConfig {
        std::string isoPath;
        std::string device;
        ISOAnalyzer::ISOStructure isoStructure;
        ISOAnalyzer::BurnStrategy strategy;
        bool persistence;
        size_t persistenceSizeMB;
        std::string persistenceFS;
        bool fastMode;
    };
    
    class IntelligentBurner {
    public:
        static bool burnWithStrategy(const BurnConfig& config);
        
    private:
        static bool burnHybridPreserve(const BurnConfig& config);
        static bool burnSmartExtract(const BurnConfig& config);
        static bool burnMultipart(const BurnConfig& config);
        static bool burnRawCopy(const BurnConfig& config);
        
        static bool createPartitionLayout(const std::string& device,
                                         const ISOAnalyzer::ISOStructure& structure,
                                         bool withPersistence,
                                         size_t persistenceSizeMB);
        
        static bool extractAndCopyISO(const std::string& isoPath,
                                     const std::string& mountPoint);
        
        static bool setupUEFIBoot(const std::string& partition,
                                 const std::string& isoPath);
        
        static bool setupLegacyBoot(const std::string& partition,
                                   const std::string& isoPath);
        
        static std::string mountPartition(const std::string& partition);
        static void unmountPartition(const std::string& mountPoint);
    };
}

#endif // SMART_BURNER_HPP
