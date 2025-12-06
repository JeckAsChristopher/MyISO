#ifndef ISO_BURNER_HPP
#define ISO_BURNER_HPP

#include <string>

namespace ISOBurner {
    enum class BurnMode {
        RAW,
        FAST
    };
    
    bool validateISO(const std::string& isoPath);
    size_t getISOSize(const std::string& isoPath);
    bool burnISO(const std::string& isoPath, const std::string& device, BurnMode mode);
    bool burnRawMode(const std::string& isoPath, const std::string& device);
    bool burnFastMode(const std::string& isoPath, const std::string& device);
}

#endif // ISO_BURNER_HPP
