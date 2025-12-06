#ifndef VERSION_HPP
#define VERSION_HPP

#include <string>

namespace Version {
    extern const std::string VERSION;
    extern const std::string AUTHOR;
    extern const std::string LICENSE;
    
    void printVersion();
    void printBanner();
}

#endif // VERSION_HPP
