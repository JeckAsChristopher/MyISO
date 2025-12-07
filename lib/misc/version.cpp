#include "misc/version.hpp"
#include "utils/colors.hpp"
#include <iostream>

namespace Version {
    const std::string VERSION = "0.5.7";
    const std::string AUTHOR = "Jeck Christopher Anog";
    const std::string LICENSE = "Open Source Project";
    
    void printVersion() {
        std::cout << Colors::bold("MyISO") << " v" << VERSION << std::endl;
        std::cout << "Author: " << AUTHOR << std::endl;
        std::cout << "License: " << LICENSE << std::endl;
    }
    
    void printBanner() {
        std::cout << Colors::cyan(R"(
 __  __       ___ ____   ___  
|  \/  |_   _|_ _/ ___| / _ \ 
| |\/| | | | || |\___ \| | | |
| |  | | |_| || | ___) | |_| |
|_|  |_|\__, |___|____/ \___/ 
        |___/                  
)") << std::endl;
        std::cout << Colors::bold("MyISO") << " v" << VERSION << " - ";
        std::cout << "Bootable USB Creator" << std::endl;
        std::cout << "Author: " << AUTHOR << std::endl;
        std::cout << std::endl;
    }
}
