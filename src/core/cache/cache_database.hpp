/// @file cache_database.hpp
/// @brief SQLite-based cache database
///
/// Provides persistent storage for thumbnail cache using SQLite.
/// Operations are thread-safe and asynchronous.

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "cache_error.hpp"
#include "thumbnail_data.hpp"

namespace nive::cache {

/// @brief Callback for async operations
template <typename T>
using AsyncCallback = std::function<void(std::expected<T, CacheError>)>;

/// @brief SQLite-based thumbnail cache database
///
/// Thread-safe database operations. Uses a dedicated I/O thread
/// for async operations to avoid blocking the UI.
///
/// Async pattern: Single Worker Thread + Callback
/// Uses a single dedicated I/O thread (not the global ThreadPool) because:
/// - SQLite connections are not safely shareable across threads
/// - Single thread serializes all DB access without additional locking
/// - Callbacks deliver results to callers without requiring future::get() blocking
class CacheDatabase {
public:
    /// @brief Open or create a cache database
    /// @param path Path to SQLite database file
    /// @param compression_level zstd compression level (0=off, 1-19=zstd)
    /// @return Database instance or error
    [[nodiscard]] static std::expected<std::unique_ptr<CacheDatabase>, CacheError>
    open(const std::filesystem::path& path, int compression_level = 3);

    ~CacheDatabase();

    // Non-copyable, non-movable
    CacheDatabase(const CacheDatabase&) = delete;
    CacheDatabase& operator=(const CacheDatabase&) = delete;
    CacheDatabase(CacheDatabase&&) = delete;
    CacheDatabase& operator=(CacheDatabase&&) = delete;

    /// @brief Check if database is open and valid
    [[nodiscard]] bool isOpen() const noexcept;

    /// @brief Get database file path
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    // ===== Synchronous Operations =====

    /// @brief Get thumbnail entry by cache key
    /// @param key Cache key (SHA256 hash)
    /// @return Thumbnail entry or error
    [[nodiscard]] std::expected<ThumbnailEntry, CacheError> get(const std::string& key);

    /// @brief Get metadata only (without pixel data)
    /// @param key Cache key
    /// @return Metadata or error
    [[nodiscard]] std::expected<ThumbnailMetadata, CacheError> getMetadata(const std::string& key);

    /// @brief Check if entry exists
    /// @param key Cache key
    [[nodiscard]] bool exists(const std::string& key);

    /// @brief Store thumbnail entry
    /// @param entry Entry to store
    /// @return Success or error
    [[nodiscard]] std::expected<void, CacheError> put(const ThumbnailEntry& entry);

    /// @brief Delete entry by key
    /// @param key Cache key
    /// @return Success or error
    [[nodiscard]] std::expected<void, CacheError> remove(const std::string& key);

    /// @brief Delete entries older than specified time
    /// @param older_than Delete entries cached before this time
    /// @return Number of deleted entries or error
    [[nodiscard]] std::expected<uint64_t, CacheError>
    removeOlderThan(std::chrono::system_clock::time_point older_than);

    /// @brief Delete entries for non-existent source files
    /// @return Number of deleted entries or error
    [[nodiscard]] std::expected<uint64_t, CacheError> removeOrphaned();

    /// @brief Get cache statistics
    [[nodiscard]] std::expected<CacheStats, CacheError> getStats();

    /// @brief Clear all entries
    /// @return Number of deleted entries or error
    [[nodiscard]] std::expected<uint64_t, CacheError> clear();

    /// @brief Vacuum database to reclaim space
    [[nodiscard]] std::expected<void, CacheError> vacuum();

    // ===== Async Operations =====

    /// @brief Get thumbnail entry asynchronously
    void getAsync(const std::string& key, AsyncCallback<ThumbnailEntry> callback);

    /// @brief Store thumbnail entry asynchronously
    void putAsync(ThumbnailEntry entry, AsyncCallback<void> callback);

    /// @brief Delete entry asynchronously
    void removeAsync(const std::string& key, AsyncCallback<void> callback);

private:
    CacheDatabase();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nive::cache
