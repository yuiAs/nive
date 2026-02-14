/// @file cache_manager.hpp
/// @brief High-level cache manager with LRU memory cache
///
/// Provides a two-tier caching system:
/// 1. LRU memory cache for fast access
/// 2. Persistent database for disk storage

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "../image/decoded_image.hpp"
#include "cache_database.hpp"
#include "cache_error.hpp"
#include "thumbnail_data.hpp"

namespace nive::cache {

/// @brief Original image resolution
struct ImageResolution {
    uint32_t width = 0;
    uint32_t height = 0;
};

/// @brief High-level cache manager
///
/// Manages a two-tier cache:
/// - Fast LRU memory cache
/// - Persistent database cache
///
/// Thread-safe for all operations.
class CacheManager {
public:
    /// @brief Create cache manager with configuration
    /// @param config Cache configuration
    /// @return Cache manager or error
    [[nodiscard]] static std::expected<std::unique_ptr<CacheManager>, CacheError>
    create(const CacheConfig& config);

    ~CacheManager();

    // Non-copyable, non-movable
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;
    CacheManager(CacheManager&&) = delete;
    CacheManager& operator=(CacheManager&&) = delete;

    /// @brief Get configuration
    [[nodiscard]] const CacheConfig& config() const noexcept;

    /// @brief Check if cache is ready
    [[nodiscard]] bool isReady() const noexcept;

    // ===== Thumbnail Operations =====

    /// @brief Get cached thumbnail for a file
    /// @param path Source file path
    /// @return Decoded thumbnail image or error
    ///
    /// Checks memory cache first, then disk cache.
    /// Returns NotFound if not cached.
    [[nodiscard]] std::expected<image::DecodedImage, CacheError>
    getThumbnail(const std::filesystem::path& path);

    /// @brief Check if thumbnail is cached
    /// @param path Source file path
    /// @return true if cached (memory or disk)
    [[nodiscard]] bool hasThumbnail(const std::filesystem::path& path);

    /// @brief Store thumbnail in cache
    /// @param path Source file path
    /// @param thumbnail Thumbnail image to cache
    /// @param original_width Original image width
    /// @param original_height Original image height
    /// @return Success or error
    ///
    /// Stores in both memory and disk cache.
    [[nodiscard]] std::expected<void, CacheError> putThumbnail(const std::filesystem::path& path,
                                                               const image::DecodedImage& thumbnail,
                                                               uint32_t original_width,
                                                               uint32_t original_height);

    /// @brief Get original image resolution from cache
    /// @param path Source file path
    /// @return Resolution if cached, nullopt otherwise
    [[nodiscard]] std::optional<ImageResolution>
    getImageResolution(const std::filesystem::path& path);

    /// @brief Remove thumbnail from cache
    /// @param path Source file path
    void removeThumbnail(const std::filesystem::path& path);

    /// @brief Get thumbnail asynchronously
    /// @param path Source file path
    /// @param callback Called with result
    void
    getThumbnailAsync(const std::filesystem::path& path,
                      std::function<void(std::expected<image::DecodedImage, CacheError>)> callback);

    /// @brief Store thumbnail asynchronously
    void putThumbnailAsync(const std::filesystem::path& path, image::DecodedImage thumbnail,
                           uint32_t original_width, uint32_t original_height,
                           std::function<void(std::expected<void, CacheError>)> callback);

    // ===== Cache Management =====

    /// @brief Clear memory cache only
    void clearMemoryCache();

    /// @brief Clear all caches (memory and disk)
    /// @return Number of entries cleared
    [[nodiscard]] std::expected<uint64_t, CacheError> clearAll();

    /// @brief Clean up expired entries
    /// @return Number of entries removed
    [[nodiscard]] std::expected<uint64_t, CacheError> cleanupExpired();

    /// @brief Remove entries for non-existent files
    /// @return Number of entries removed
    [[nodiscard]] std::expected<uint64_t, CacheError> cleanupOrphaned();

    /// @brief Enforce size limits (evict oldest entries)
    /// @return Number of entries evicted
    [[nodiscard]] std::expected<uint64_t, CacheError> enforceLimits();

    /// @brief Get cache statistics
    [[nodiscard]] CacheStats getStats() const;

    /// @brief Compact database (vacuum)
    [[nodiscard]] std::expected<void, CacheError> compact();

    /// @brief Prefetch thumbnails for files in directory
    /// @param directory Directory path
    /// @param callback Called for each thumbnail loaded
    ///
    /// Loads existing cache entries into memory cache.
    void prefetch(const std::filesystem::path& directory,
                  std::function<void(const std::filesystem::path&)> callback = nullptr);

private:
    CacheManager();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nive::cache
