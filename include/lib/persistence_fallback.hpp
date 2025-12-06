#ifndef PERSISTENCE_FALLBACK_HPP
#define PERSISTENCE_FALLBACK_HPP

#include "lib/fs_supports.hpp"
#include <string>

namespace PersistenceFallback {
    bool createFileBased(
        const std::string& mountPoint,
        size_t sizeInMB,
        const std::string& label = "casper-rw"
    );
    
    bool setupFallbackPersistence(
        const std::string& isoPath,
        const std::string& device,
        size_t persistenceSizeMB
    );
}

#endif // PERSISTENCE_FALLBACK_HPP
