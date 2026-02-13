/// @file cache_error.hpp
/// @brief Cache system error types

#pragma once

#include <string_view>

namespace nive::cache {

/// @brief Cache operation errors
enum class CacheError {
    NotFound,          // Cache entry not found
    Expired,           // Cache entry has expired
    DatabaseError,     // SQLite error
    IoError,           // File I/O error
    CorruptedData,     // Data integrity check failed
    OutOfMemory,       // Memory allocation failed
    InvalidPath,       // Invalid cache path
    AlreadyExists,     // Entry already exists
    CompressionError,  // zstd compression/decompression failed
};

/// @brief Get string representation of cache error
[[nodiscard]] constexpr std::string_view to_string(CacheError error) noexcept {
    switch (error) {
    case CacheError::NotFound:
        return "Cache entry not found";
    case CacheError::Expired:
        return "Cache entry has expired";
    case CacheError::DatabaseError:
        return "Database error";
    case CacheError::IoError:
        return "I/O error";
    case CacheError::CorruptedData:
        return "Corrupted cache data";
    case CacheError::OutOfMemory:
        return "Out of memory";
    case CacheError::InvalidPath:
        return "Invalid cache path";
    case CacheError::AlreadyExists:
        return "Entry already exists";
    }
    return "Unknown cache error";
}

}  // namespace nive::cache
