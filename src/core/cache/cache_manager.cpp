/// @file cache_manager.cpp
/// @brief Cache manager implementation with LRU memory cache

#include "cache_manager.hpp"

#include <chrono>
#include <list>
#include <mutex>
#include <unordered_map>

namespace nive::cache {

/// @brief LRU cache implementation
template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(size_t capacity) : capacity_(capacity) {}

    [[nodiscard]] std::optional<Value> get(const Key& key) {
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return std::nullopt;
        }

        // Move to front (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
        return it->second->second;
    }

    void put(const Key& key, Value value) {
        auto it = cache_map_.find(key);

        if (it != cache_map_.end()) {
            // Update existing
            it->second->second = std::move(value);
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
            return;
        }

        // Evict if at capacity
        if (cache_map_.size() >= capacity_) {
            auto last = cache_list_.end();
            --last;
            cache_map_.erase(last->first);
            cache_list_.pop_back();
        }

        // Insert new
        cache_list_.emplace_front(key, std::move(value));
        cache_map_[key] = cache_list_.begin();
    }

    void remove(const Key& key) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
        }
    }

    [[nodiscard]] bool contains(const Key& key) const {
        return cache_map_.find(key) != cache_map_.end();
    }

    void clear() {
        cache_list_.clear();
        cache_map_.clear();
    }

    [[nodiscard]] size_t size() const { return cache_map_.size(); }

    [[nodiscard]] size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    std::list<std::pair<Key, Value>> cache_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
};

/// @brief Cache manager implementation
class CacheManager::Impl {
public:
    explicit Impl(CacheConfig config)
        : config_(std::move(config)), memory_cache_(config_.memory_cache_size) {}

    [[nodiscard]] bool initialize() {
        auto db_result = CacheDatabase::open(config_.database_path, config_.compression_level);
        if (!db_result) {
            return false;
        }
        database_ = std::move(*db_result);
        return true;
    }

    [[nodiscard]] const CacheConfig& config() const noexcept { return config_; }

    [[nodiscard]] bool isReady() const noexcept { return database_ && database_->isOpen(); }

    [[nodiscard]] std::expected<image::DecodedImage, CacheError>
    getThumbnail(const std::filesystem::path& path) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            return std::unexpected(CacheError::InvalidPath);
        }

        // Check memory cache first
        {
            std::lock_guard lock(memory_mutex_);
            auto cached = memory_cache_.get(key);
            if (cached) {
                ++stats_.hits;
                return convert_entry_to_image(*cached);
            }
        }

        // Check disk cache
        auto result = database_->get(key);
        if (!result) {
            ++stats_.misses;
            return std::unexpected(result.error());
        }

        ++stats_.hits;

        // Add to memory cache
        {
            std::lock_guard lock(memory_mutex_);
            memory_cache_.put(key, *result);
        }

        return convert_entry_to_image(*result);
    }

    [[nodiscard]] bool hasThumbnail(const std::filesystem::path& path) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            return false;
        }

        // Check memory cache
        {
            std::lock_guard lock(memory_mutex_);
            if (memory_cache_.contains(key)) {
                return true;
            }
        }

        // Check disk cache
        return database_->exists(key);
    }

    [[nodiscard]] std::expected<void, CacheError> putThumbnail(const std::filesystem::path& path,
                                                               const image::DecodedImage& thumbnail,
                                                               uint32_t original_width,
                                                               uint32_t original_height) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            return std::unexpected(CacheError::InvalidPath);
        }

        auto entry = buildEntry(key, path, thumbnail, original_width, original_height);

        // Store in memory cache
        {
            std::lock_guard lock(memory_mutex_);
            memory_cache_.put(key, entry);
        }

        // Store in disk cache
        return database_->put(entry);
    }

    [[nodiscard]] std::optional<ImageResolution>
    getImageResolution(const std::filesystem::path& path) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            return std::nullopt;
        }

        // Check memory cache first (O(1) after getThumbnail loaded the entry)
        {
            std::lock_guard lock(memory_mutex_);
            auto cached = memory_cache_.get(key);
            if (cached && cached->metadata.original_width > 0) {
                return ImageResolution{cached->metadata.original_width,
                                       cached->metadata.original_height};
            }
        }

        // Fall back to disk cache
        auto result = database_->get(key);
        if (result && result->metadata.original_width > 0) {
            return ImageResolution{result->metadata.original_width,
                                   result->metadata.original_height};
        }

        return std::nullopt;
    }

    void removeThumbnail(const std::filesystem::path& path) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            return;
        }

        // Remove from memory cache
        {
            std::lock_guard lock(memory_mutex_);
            memory_cache_.remove(key);
        }

        // Remove from disk cache (ignore result)
        (void)database_->remove(key);
    }

    void getThumbnailAsync(
        const std::filesystem::path& path,
        std::function<void(std::expected<image::DecodedImage, CacheError>)> callback) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            callback(std::unexpected(CacheError::InvalidPath));
            return;
        }

        // Check memory cache first (synchronously)
        {
            std::lock_guard lock(memory_mutex_);
            auto cached = memory_cache_.get(key);
            if (cached) {
                ++stats_.hits;
                callback(convert_entry_to_image(*cached));
                return;
            }
        }

        // Load from disk asynchronously
        database_->getAsync(key, [this, key, callback = std::move(callback)](
                                     std::expected<ThumbnailEntry, CacheError> result) {
            if (!result) {
                ++stats_.misses;
                callback(std::unexpected(result.error()));
                return;
            }

            ++stats_.hits;

            // Add to memory cache
            {
                std::lock_guard lock(memory_mutex_);
                memory_cache_.put(key, *result);
            }

            callback(convert_entry_to_image(*result));
        });
    }

    void putThumbnailAsync(const std::filesystem::path& path, image::DecodedImage thumbnail,
                           uint32_t original_width, uint32_t original_height,
                           std::function<void(std::expected<void, CacheError>)> callback) {
        std::string key = generateCacheKey(path);
        if (key.empty()) {
            callback(std::unexpected(CacheError::InvalidPath));
            return;
        }

        auto entry = buildEntry(key, path, thumbnail, original_width, original_height);

        // Store in memory cache immediately
        {
            std::lock_guard lock(memory_mutex_);
            memory_cache_.put(key, entry);
        }

        // Store in disk cache asynchronously
        database_->putAsync(std::move(entry), std::move(callback));
    }

    void clearMemoryCache() {
        std::lock_guard lock(memory_mutex_);
        memory_cache_.clear();
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> clearAll() {
        clearMemoryCache();
        return database_->clear();
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> cleanupExpired() {
        if (!config_.retention_period) {
            return 0;
        }

        auto cutoff = std::chrono::system_clock::now() - *config_.retention_period;
        auto result = database_->removeOlderThan(cutoff);
        if (result) {
            stats_.evictions += *result;
        }
        return result;
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> cleanupOrphaned() {
        auto result = database_->removeOrphaned();
        if (result) {
            stats_.evictions += *result;
        }
        return result;
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> enforceLimits() {
        auto stats_result = database_->getStats();
        if (!stats_result) {
            return std::unexpected(stats_result.error());
        }

        uint64_t removed = 0;

        // If over entry limit or size limit, clean up expired entries first
        if (stats_result->total_entries > config_.max_entries ||
            stats_result->total_size_bytes > config_.max_size_bytes) {
            auto expired = cleanupExpired();
            if (expired) {
                removed += *expired;
            }

            // Check again
            stats_result = database_->getStats();
            if (!stats_result) {
                return removed;
            }
        }

        // TODO: If still over limit, evict oldest entries
        // This would require a more sophisticated approach

        return removed;
    }

    [[nodiscard]] CacheStats getStats() const {
        CacheStats stats = stats_;
        if (database_) {
            auto db_stats = database_->getStats();
            if (db_stats) {
                stats.total_entries = db_stats->total_entries;
                stats.total_size_bytes = db_stats->total_size_bytes;
                stats.oldest_entry = db_stats->oldest_entry;
                stats.newest_entry = db_stats->newest_entry;
            }
        }
        return stats;
    }

    [[nodiscard]] std::expected<void, CacheError> compact() { return database_->vacuum(); }

    void prefetch(const std::filesystem::path& directory,
                  std::function<void(const std::filesystem::path&)> callback) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
            if (!entry.is_regular_file())
                continue;

            std::string key = generateCacheKey(entry.path());
            if (key.empty())
                continue;

            // Check if already in memory
            {
                std::lock_guard lock(memory_mutex_);
                if (memory_cache_.contains(key)) {
                    if (callback)
                        callback(entry.path());
                    continue;
                }
            }

            // Try to load from disk
            auto result = database_->get(key);
            if (result) {
                std::lock_guard lock(memory_mutex_);
                memory_cache_.put(key, std::move(*result));
                if (callback)
                    callback(entry.path());
            }
        }
    }

private:
    [[nodiscard]] static ThumbnailEntry buildEntry(const std::string& key,
                                                   const std::filesystem::path& path,
                                                   const image::DecodedImage& thumbnail,
                                                   uint32_t original_width,
                                                   uint32_t original_height) {
        ThumbnailEntry entry;
        entry.metadata.file_hash = key;
        entry.metadata.source_path = path;
        entry.metadata.width = thumbnail.width();
        entry.metadata.height = thumbnail.height();
        entry.metadata.original_width = original_width;
        entry.metadata.original_height = original_height;
        entry.metadata.cached_at = std::chrono::system_clock::now();

        std::error_code ec;
        auto mtime = std::filesystem::last_write_time(path, ec);
        if (!ec) {
            entry.metadata.source_mtime = std::chrono::clock_cast<std::chrono::system_clock>(mtime);
        }

        entry.data.assign(thumbnail.pixels().begin(), thumbnail.pixels().end());
        entry.metadata.data_size = entry.data.size();
        return entry;
    }

    [[nodiscard]] static image::DecodedImage convert_entry_to_image(const ThumbnailEntry& entry) {
        std::vector<uint8_t> pixels(entry.data.begin(), entry.data.end());
        uint32_t stride = (entry.metadata.width * 4 + 3) & ~3u;  // BGRA32

        return image::DecodedImage(entry.metadata.width, entry.metadata.height,
                                   image::PixelFormat::BGRA32, stride, std::move(pixels));
    }

    CacheConfig config_;
    std::unique_ptr<CacheDatabase> database_;
    LruCache<std::string, ThumbnailEntry> memory_cache_;
    std::mutex memory_mutex_;
    mutable CacheStats stats_;
};

// Public interface

std::expected<std::unique_ptr<CacheManager>, CacheError>
CacheManager::create(const CacheConfig& config) {
    auto manager = std::unique_ptr<CacheManager>(new CacheManager());
    manager->impl_ = std::make_unique<Impl>(config);

    if (!manager->impl_->initialize()) {
        return std::unexpected(CacheError::DatabaseError);
    }

    return manager;
}

CacheManager::CacheManager() = default;
CacheManager::~CacheManager() = default;

const CacheConfig& CacheManager::config() const noexcept {
    return impl_->config();
}

bool CacheManager::isReady() const noexcept {
    return impl_ && impl_->isReady();
}

std::expected<image::DecodedImage, CacheError>
CacheManager::getThumbnail(const std::filesystem::path& path) {
    return impl_->getThumbnail(path);
}

bool CacheManager::hasThumbnail(const std::filesystem::path& path) {
    return impl_->hasThumbnail(path);
}

std::expected<void, CacheError> CacheManager::putThumbnail(const std::filesystem::path& path,
                                                           const image::DecodedImage& thumbnail,
                                                           uint32_t original_width,
                                                           uint32_t original_height) {
    return impl_->putThumbnail(path, thumbnail, original_width, original_height);
}

std::optional<ImageResolution>
CacheManager::getImageResolution(const std::filesystem::path& path) {
    return impl_->getImageResolution(path);
}

void CacheManager::removeThumbnail(const std::filesystem::path& path) {
    impl_->removeThumbnail(path);
}

void CacheManager::getThumbnailAsync(
    const std::filesystem::path& path,
    std::function<void(std::expected<image::DecodedImage, CacheError>)> callback) {
    impl_->getThumbnailAsync(path, std::move(callback));
}

void CacheManager::putThumbnailAsync(
    const std::filesystem::path& path, image::DecodedImage thumbnail, uint32_t original_width,
    uint32_t original_height, std::function<void(std::expected<void, CacheError>)> callback) {
    impl_->putThumbnailAsync(path, std::move(thumbnail), original_width, original_height,
                             std::move(callback));
}

void CacheManager::clearMemoryCache() {
    impl_->clearMemoryCache();
}

std::expected<uint64_t, CacheError> CacheManager::clearAll() {
    return impl_->clearAll();
}

std::expected<uint64_t, CacheError> CacheManager::cleanupExpired() {
    return impl_->cleanupExpired();
}

std::expected<uint64_t, CacheError> CacheManager::cleanupOrphaned() {
    return impl_->cleanupOrphaned();
}

std::expected<uint64_t, CacheError> CacheManager::enforceLimits() {
    return impl_->enforceLimits();
}

CacheStats CacheManager::getStats() const {
    return impl_->getStats();
}

std::expected<void, CacheError> CacheManager::compact() {
    return impl_->compact();
}

void CacheManager::prefetch(const std::filesystem::path& directory,
                            std::function<void(const std::filesystem::path&)> callback) {
    impl_->prefetch(directory, std::move(callback));
}

}  // namespace nive::cache
