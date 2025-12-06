#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <string>
#include <exception>

class MyISOException : public std::exception {
private:
    std::string message;
    
public:
    explicit MyISOException(const std::string& msg);
    const char* what() const noexcept override;
};

class PermissionError : public MyISOException {
public:
    explicit PermissionError(const std::string& msg);
};

class DeviceError : public MyISOException {
public:
    explicit DeviceError(const std::string& device, const std::string& cause);
};

class FileError : public MyISOException {
public:
    explicit FileError(const std::string& file, const std::string& cause);
};

class FilesystemError : public MyISOException {
public:
    explicit FilesystemError(const std::string& msg);
};

namespace ErrorHandler {
    void handleFatalError(const std::string& device, const std::string& cause);
    void checkPrivileges();
}

#endif // ERRORS_HPP
