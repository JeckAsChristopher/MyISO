#include "lib/iso_burner.hpp"
#include "lib/errors.hpp"
#include "utils/logs.hpp"
#include "utils/progress_bar.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

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
        
        switch (mode) {
            case BurnMode::RAW:
                return burnRawMode(isoPath, device);
            case BurnMode::FAST:
                return burnFastMode(isoPath, device);
            default:
                throw MyISOException("Unknown burn mode");
        }
    }
    
    bool burnRawMode(const std::string& isoPath, const std::string& device) {
        Logs::info("Burning ISO in RAW mode (dd)");
        
        std::ifstream input(isoPath, std::ios::binary);
        if (!input.is_open()) {
            throw FileError(isoPath, "Cannot open ISO file");
        }
        
        std::ofstream output(device, std::ios::binary);
        if (!output.is_open()) {
            throw DeviceError(device, "Cannot open device for writing");
        }
        
        size_t totalSize = getISOSize(isoPath);
        ProgressBar progress(totalSize, "Writing ISO");
        
        const size_t BUFFER_SIZE = 1024 * 1024;
        char* buffer = new char[BUFFER_SIZE];
        size_t bytesWritten = 0;
        
        try {
            while (input.read(buffer, BUFFER_SIZE) || input.gcount() > 0) {
                size_t bytesRead = input.gcount();
                output.write(buffer, bytesRead);
                
                if (!output.good()) {
                    delete[] buffer;
                    throw DeviceError(device, "Write operation failed");
                }
                
                bytesWritten += bytesRead;
                progress.update(bytesWritten);
            }
            
            progress.finish();
            delete[] buffer;
            
        } catch (...) {
            delete[] buffer;
            throw;
        }
        
        input.close();
        output.close();
        
        sync();
        
        Logs::success("ISO burned successfully in RAW mode");
        return true;
    }
    
    bool burnFastMode(const std::string& isoPath, const std::string& device) {
        Logs::info("Burning ISO in FAST mode (optimized dd)");
        
        size_t totalSize = getISOSize(isoPath);
        
        std::string cmd = "dd if=" + isoPath + " of=" + device + 
                          " bs=4M status=progress conv=fsync 2>&1";
        
        Logs::info("Executing: dd with 4MB block size");
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw DeviceError(device, "Failed to execute dd command");
        }
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        }
        
        int status = pclose(pipe);
        
        if (status != 0) {
            throw DeviceError(device, "dd command failed with status " + std::to_string(status));
        }
        
        Logs::success("ISO burned successfully in FAST mode");
        return true;
    }
}
