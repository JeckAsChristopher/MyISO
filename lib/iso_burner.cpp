#include "lib/iso_burner.hpp"
#include "lib/errors.hpp"
#include "lib/bootloader.hpp"
#include "utils/logs.hpp"
#include "utils/progress_bar.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>

namespace ISOBurner {
    
    bool validateISO(const std::string& isoPath) {
        std::ifstream file(isoPath, std::ios::binary);
        
        if (!file.is_open()) {
            throw FileError(isoPath, "Cannot open file");
        }
        
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        
        if (fileSize < 1024) {
            throw FileError(isoPath, "File too small to be a valid ISO");
        }
        
        char buffer[2048];
        file.seekg(32768, std::ios::beg);
        file.read(buffer, 2048);
        
        std::string content(buffer, 2048);
        bool isISO = (content.find("CD001") != std::string::npos);
        
        if (!isISO) {
            Logs::warning("File may not be a valid ISO 9660 image");
        }
        
        file.close();
        return true;
    }
    
    size_t getISOSize(const std::string& isoPath) {
        struct stat st;
        if (stat(isoPath.c_str(), &st) != 0) {
            throw FileError(isoPath, "Cannot get file size");
        }
        return st.st_size;
    }
    
    bool burnISO(const std::string& isoPath, const std::string& device, BurnMode mode) {
        validateISO(isoPath);
        
        bool success = false;
        
        switch (mode) {
            case BurnMode::RAW:
                success = burnRawMode(isoPath, device);
                break;
            case BurnMode::FAST:
                success = burnFastMode(isoPath, device);
                break;
            default:
                throw MyISOException("Unknown burn mode");
        }
        
        if (success) {
            Logs::info("Installing bootloader...");
            Bootloader::installBootloader(device, isoPath);
        }
        
        return success;
    }
    
    bool burnRawMode(const std::string& isoPath, const std::string& device) {
        Logs::info("Burning ISO in RAW mode with optimized I/O");
        
        int inputFd = open(isoPath.c_str(), O_RDONLY);
        if (inputFd < 0) {
            throw FileError(isoPath, "Cannot open ISO file");
        }
        
        int outputFd = open(device.c_str(), O_WRONLY | O_SYNC | O_DIRECT);
        if (outputFd < 0) {
            outputFd = open(device.c_str(), O_WRONLY | O_SYNC);
            if (outputFd < 0) {
                close(inputFd);
                throw DeviceError(device, "Cannot open device for writing");
            }
        }
        
        size_t totalSize = getISOSize(isoPath);
        ProgressBar progress(totalSize, "Writing ISO");
        
        const size_t BUFFER_SIZE = 4 * 1024 * 1024;
        
        void* alignedBuffer;
        if (posix_memalign(&alignedBuffer, 4096, BUFFER_SIZE) != 0) {
            close(inputFd);
            close(outputFd);
            throw MyISOException("Failed to allocate aligned buffer");
        }
        
        char* buffer = static_cast<char*>(alignedBuffer);
        size_t bytesWritten = 0;
        
        try {
            ssize_t bytesRead;
            while ((bytesRead = read(inputFd, buffer, BUFFER_SIZE)) > 0) {
                size_t totalWritten = 0;
                
                while (totalWritten < static_cast<size_t>(bytesRead)) {
                    ssize_t written = write(outputFd, buffer + totalWritten, 
                                           bytesRead - totalWritten);
                    
                    if (written < 0) {
                        free(alignedBuffer);
                        close(inputFd);
                        close(outputFd);
                        throw DeviceError(device, "Write operation failed");
                    }
                    
                    totalWritten += written;
                }
                
                bytesWritten += bytesRead;
                progress.update(bytesWritten);
            }
            
            progress.finish();
            free(alignedBuffer);
            
        } catch (...) {
            free(alignedBuffer);
            close(inputFd);
            close(outputFd);
            throw;
        }
        
        fsync(outputFd);
        close(inputFd);
        close(outputFd);
        
        sync();
        
        Logs::success("ISO burned successfully in RAW mode");
        return true;
    }
    
    bool burnFastMode(const std::string& isoPath, const std::string& device) {
        Logs::info("Burning ISO in FAST mode with zero-copy I/O");
        
        int inputFd = open(isoPath.c_str(), O_RDONLY);
        if (inputFd < 0) {
            throw FileError(isoPath, "Cannot open ISO file");
        }
        
        int outputFd = open(device.c_str(), O_WRONLY | O_SYNC);
        if (outputFd < 0) {
            close(inputFd);
            throw DeviceError(device, "Cannot open device for writing");
        }
        
        size_t totalSize = getISOSize(isoPath);
        ProgressBar progress(totalSize, "Fast Writing");
        
        size_t bytesWritten = 0;
        const size_t CHUNK_SIZE = 16 * 1024 * 1024;
        
        try {
            while (bytesWritten < totalSize) {
                size_t toWrite = std::min(CHUNK_SIZE, totalSize - bytesWritten);
                
                ssize_t sent = sendfile(outputFd, inputFd, nullptr, toWrite);
                
                if (sent <= 0) {
                    if (errno == EINVAL || errno == ENOSYS) {
                        close(inputFd);
                        close(outputFd);
                        Logs::info("sendfile not supported, falling back to RAW mode");
                        return burnRawMode(isoPath, device);
                    }
                    
                    close(inputFd);
                    close(outputFd);
                    throw DeviceError(device, "Fast write operation failed");
                }
                
                bytesWritten += sent;
                progress.update(bytesWritten);
            }
            
            progress.finish();
            
        } catch (...) {
            close(inputFd);
            close(outputFd);
            throw;
        }
        
        fsync(outputFd);
        close(inputFd);
        close(outputFd);
        
        sync();
        
        Logs::success("ISO burned successfully in FAST mode");
        return true;
    }
}
