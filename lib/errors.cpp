#include "lib/errors.hpp"
#include "utils/colors.hpp"
#include "utils/logs.hpp"
#include <unistd.h>
#include <iostream>

MyISOException::MyISOException(const std::string& msg) : message(msg) {}

const char* MyISOException::what() const noexcept {
    return message.c_str();
}

PermissionError::PermissionError(const std::string& msg) 
    : MyISOException(msg) {}

DeviceError::DeviceError(const std::string& device, const std::string& cause)
    : MyISOException("Device error on " + device + ": " + cause) {}

FileError::FileError(const std::string& file, const std::string& cause)
    : MyISOException("File error with " + file + ": " + cause) {}

FilesystemError::FilesystemError(const std::string& msg)
    : MyISOException("Filesystem error: " + msg) {}

namespace ErrorHandler {
    void handleFatalError(const std::string& device, const std::string& cause) {
        std::string devName = device;
        if (devName.find("/dev/") == 0) {
            devName = devName.substr(5);
        }
        
        Logs::fatal("Fatal Error: Fail writing at /dev/" + devName + ", cause: " + cause);
    }
    
    void checkPrivileges() {
        if (geteuid() != 0) {
            std::cerr << Colors::red("This is a privilege tool, to access this, use sudo.") << std::endl;
            throw PermissionError("Root privileges required");
        }
    }
}
