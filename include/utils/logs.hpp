#ifndef LOGS_HPP
#define LOGS_HPP

#include <string>

namespace Logs {
    void info(const std::string& message);
    void success(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    void debug(const std::string& message);
}

#endif // LOGS_HPP
