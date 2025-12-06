#include "utils/logs.hpp"
#include "utils/colors.hpp"
#include <iostream>

namespace Logs {
    void info(const std::string& message) {
        std::cout << Colors::cyan("[INFO] ") << message << std::endl;
    }
    
    void success(const std::string& message) {
        std::cout << Colors::green("[SUCCESS] ") << message << std::endl;
    }
    
    void warning(const std::string& message) {
        std::cout << Colors::yellow("[WARNING] ") << message << std::endl;
    }
    
    void error(const std::string& message) {
        std::cerr << Colors::red("[ERROR] ") << message << std::endl;
    }
    
    void fatal(const std::string& message) {
        std::cerr << Colors::bold(Colors::red("[FATAL] ")) << message << std::endl;
    }
    
    void debug(const std::string& message) {
        std::cout << Colors::blue("[DEBUG] ") << message << std::endl;
    }
}
