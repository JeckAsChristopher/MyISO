#ifndef DEV_HANDLER_HPP
#define DEV_HANDLER_HPP

#include <string>
#include <cstddef>

namespace DeviceHandler {
    bool validateDevice(const std::string& device);
    bool isDeviceMounted(const std::string& device);
    bool unmountDevice(const std::string& device);
    bool wipeDevice(const std::string& device);
    size_t getDeviceSize(const std::string& device);
    bool createPartitionTable(const std::string& device);
    std::string createPartition(const std::string& device, size_t sizeInMB);
    bool syncDevice(const std::string& device);
}

#endif // DEV_HANDLER_HPP
