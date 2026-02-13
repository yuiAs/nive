/// @file thumbnail_data.hpp
/// @brief Thumbnail cache data structures

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nive::cache {

/// @brief Thumbnail metadata stored in cache
struct ThumbnailMetadata {
    std::string file_hash;                               // SHA256 of source file path + mtime
    std::filesystem::path source_path;                   // Original file path
    uint32_t width = 0;                                  // Thumbnail width
    uint32_t height = 0;                                 // Thumbnail height
    uint32_t original_width = 0;                         // Original image width
    uint32_t original_height = 0;                        // Original image height
    std::chrono::system_clock::time_point source_mtime;  // Source file modification time
    std::chrono::system_clock::time_point cached_at;     // When this was cached
    uint64_t data_size = 0;                              // Size of thumbnail data in bytes
};

/// @brief Complete thumbnail cache entry
struct ThumbnailEntry {
    ThumbnailMetadata metadata;
    std::vector<uint8_t> data;  // Raw BGRA32 pixel data

    [[nodiscard]] bool valid() const noexcept {
        return !metadata.file_hash.empty() && metadata.width > 0 && metadata.height > 0 &&
               !data.empty();
    }
};

/// @brief Cache statistics
struct CacheStats {
    uint64_t total_entries = 0;
    uint64_t total_size_bytes = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    std::chrono::system_clock::time_point oldest_entry;
    std::chrono::system_clock::time_point newest_entry;

    [[nodiscard]] double hitRate() const noexcept {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
};

/// @brief Cache configuration
struct CacheConfig {
    std::filesystem::path database_path;
    uint64_t max_size_bytes = 500 * 1024 * 1024;  // 500 MB default
    uint64_t max_entries = 10000;
    std::optional<std::chrono::hours> retention_period;  // nullopt = never expire
    int compression_level = 3;                           // 0=off, 1-19=zstd levels
    size_t memory_cache_size = 100;                      // LRU memory cache size
};

/// @brief Generate cache key from file path and modification time
/// @param path File path
/// @param mtime File modification time
/// @return SHA256 hash as hex string
[[nodiscard]] std::string generateCacheKey(const std::filesystem::path& path,
                                           std::chrono::system_clock::time_point mtime);

/// @brief Generate cache key from file path (auto-detects mtime)
/// @param path File path
/// @return SHA256 hash as hex string, or empty on error
[[nodiscard]] std::string generateCacheKey(const std::filesystem::path& path);

}  // namespace nive::cache
