#ifndef PERSISTENCE_HPP
#define PERSISTENCE_HPP

#include "lib/fs_supports.hpp"
#include "lib/mbr_gpt.hpp"
#include <string>

namespace Persistence {
    bool createPersistencePartition(
        const std::string& device,
        size_t sizeInMB,
        FilesystemSupport::FSType fsType
    );
    
    bool setupPersistence(
        const std::string& isoPath,
        const std::string& device,
        size_t persistenceSizeMB,
        FilesystemSupport::FSType fsType,
        BootStructures::TableType tableType = BootStructures::TableType::MBR
    );
    
    size_t calculateOptimalSize(size_t isoSize, size_t deviceSize);
}

#endif // PERSISTENCE_HPP
