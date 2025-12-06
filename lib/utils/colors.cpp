#include "utils/colors.hpp"

namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    
    std::string colorize(const std::string& text, const std::string& color) {
        return color + text + RESET;
    }
    
    std::string red(const std::string& text) {
        return RED + text + RESET;
    }
    
    std::string green(const std::string& text) {
        return GREEN + text + RESET;
    }
    
    std::string yellow(const std::string& text) {
        return YELLOW + text + RESET;
    }
    
    std::string blue(const std::string& text) {
        return BLUE + text + RESET;
    }
    
    std::string cyan(const std::string& text) {
        return CYAN + text + RESET;
    }
    
    std::string white(const std::string& text) {
        return WHITE + text + RESET;
    }
    
    std::string bold(const std::string& text) {
        return BOLD + text + RESET;
    }
}
