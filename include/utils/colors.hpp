#ifndef COLORS_HPP
#define COLORS_HPP

#include <string>

namespace Colors {
    extern const std::string RESET;
    extern const std::string RED;
    extern const std::string GREEN;
    extern const std::string YELLOW;
    extern const std::string BLUE;
    extern const std::string MAGENTA;
    extern const std::string CYAN;
    extern const std::string WHITE;
    extern const std::string BOLD;
    
    std::string colorize(const std::string& text, const std::string& color);
    std::string red(const std::string& text);
    std::string green(const std::string& text);
    std::string yellow(const std::string& text);
    std::string blue(const std::string& text);
    std::string cyan(const std::string& text);
    std::string bold(const std::string& text);
}

#endif // COLORS_HPP
