/// @file cache_database.cpp
/// @brief SQLite-based cache database implementation

#include "cache_database.hpp"

#include <mutex>
#include <queue>
#include <thread>

#include <sqlite3.h>

#include "../util/logger.hpp"
#include "../util/string_utils.hpp"

#ifdef NIVE_HAS_ZSTD
    #include <zstd.h>
#endif

namespace nive::cache {

namespace {

// Convert SQLite error to CacheError
CacheError sqlite_to_cache_error(int rc) {
    switch (rc) {
    case SQLITE_NOTFOUND:
        return CacheError::NotFound;
    case SQLITE_CORRUPT:
    case SQLITE_MISMATCH:
        return CacheError::CorruptedData;
    case SQLITE_IOERR:
    case SQLITE_CANTOPEN:
    case SQLITE_FULL:
    case SQLITE_READONLY:
        return CacheError::IoError;
    case SQLITE_NOMEM:
        return CacheError::OutOfMemory;
    default:
        return CacheError::DatabaseError;
    }
}

// RAII wrapper for sqlite3_stmt
class SqliteStatement {
public:
    SqliteStatement() = default;
    ~SqliteStatement() { finalize(); }

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    SqliteStatement(SqliteStatement&& other) noexcept : stmt_(other.stmt_) {
        other.stmt_ = nullptr;
    }

    SqliteStatement& operator=(SqliteStatement&& other) noexcept {
        if (this != &other) {
            finalize();
            stmt_ = other.stmt_;
            other.stmt_ = nullptr;
        }
        return *this;
    }

    bool prepare(sqlite3* db, const char* sql) {
        finalize();
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("SQLite prepare failed: {} - {}", rc, sqlite3_errmsg(db));
            return false;
        }
        return true;
    }

    void finalize() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
            stmt_ = nullptr;
        }
    }

    void reset() {
        if (stmt_) {
            sqlite3_reset(stmt_);
            sqlite3_clear_bindings(stmt_);
        }
    }

    sqlite3_stmt* get() const { return stmt_; }
    operator sqlite3_stmt*() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// Schema version for cache invalidation
constexpr int CURRENT_SCHEMA_VERSION = 2;  // v2: added zstd compression

#ifdef NIVE_HAS_ZSTD
// zstd magic bytes: 0x28 0xB5 0x2F 0xFD
constexpr uint8_t ZSTD_MAGIC[] = {0x28, 0xB5, 0x2F, 0xFD};

bool is_zstd_compressed(const std::vector<uint8_t>& data) {
    if (data.size() < 4)
        return false;
    return std::memcmp(data.data(), ZSTD_MAGIC, 4) == 0;
}

std::expected<std::vector<uint8_t>, CacheError> compress_data(const std::vector<uint8_t>& data,
                                                              int level) {
    if (level <= 0 || data.empty()) {
        return data;  // No compression
    }

    size_t bound = ZSTD_compressBound(data.size());
    std::vector<uint8_t> compressed(bound);

    size_t compressed_size =
        ZSTD_compress(compressed.data(), compressed.size(), data.data(), data.size(), level);

    if (ZSTD_isError(compressed_size)) {
        LOG_ERROR("zstd compression failed: {}", ZSTD_getErrorName(compressed_size));
        return std::unexpected(CacheError::CompressionError);
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::expected<std::vector<uint8_t>, CacheError> decompress_data(const std::vector<uint8_t>& data) {
    if (data.empty() || !is_zstd_compressed(data)) {
        return data;  // Not compressed or empty
    }

    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data.data(), data.size());
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
        decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        LOG_ERROR("zstd: cannot determine decompressed size");
        return std::unexpected(CacheError::CompressionError);
    }

    std::vector<uint8_t> decompressed(static_cast<size_t>(decompressed_size));

    size_t result =
        ZSTD_decompress(decompressed.data(), decompressed.size(), data.data(), data.size());

    if (ZSTD_isError(result)) {
        LOG_ERROR("zstd decompression failed: {}", ZSTD_getErrorName(result));
        return std::unexpected(CacheError::CompressionError);
    }

    decompressed.resize(result);
    return decompressed;
}
#endif  // NIVE_HAS_ZSTD

}  // namespace

/// @brief SQLite cache database implementation
class CacheDatabase::Impl {
public:
    Impl(std::filesystem::path db_path, int compression_level)
        : db_path_(std::move(db_path)), compression_level_(compression_level), running_(true) {
        // Start async worker thread
        worker_ = std::jthread([this](std::stop_token stop_token) { worker_thread(stop_token); });
    }

    ~Impl() {
        running_ = false;
        cv_.notify_all();

        // Finalize all prepared statements before closing database
        stmt_get_.finalize();
        stmt_get_metadata_.finalize();
        stmt_exists_.finalize();
        stmt_put_.finalize();
        stmt_remove_.finalize();
        stmt_remove_older_than_.finalize();
        stmt_get_all_paths_.finalize();
        stmt_get_stats_.finalize();
        stmt_clear_.finalize();
        stmt_count_.finalize();

        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    [[nodiscard]] bool initialize() {
        std::lock_guard lock(db_mutex_);

        // Ensure parent directory exists
        std::error_code ec;
        std::filesystem::create_directories(db_path_.parent_path(), ec);
        if (ec) {
            LOG_ERROR("Failed to create cache directory: {}", ec.message());
            return false;
        }

        // Open database
        int rc = sqlite3_open(pathToUtf8(db_path_).c_str(), &db_);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to open cache database: {}", sqlite3_errmsg(db_));
            return false;
        }

        // Configure database
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);

        // Create tables
        if (!create_tables()) {
            return false;
        }

        // Prepare statements
        if (!prepare_statements()) {
            return false;
        }

        LOG_INFO("Cache database opened: {}", pathToUtf8(db_path_));
        return true;
    }

    [[nodiscard]] bool isOpen() const noexcept { return db_ != nullptr; }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return db_path_; }

    [[nodiscard]] std::expected<ThumbnailEntry, CacheError> get(const std::string& key) {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        stmt_get_.reset();

        // Bind cache_key
        sqlite3_bind_text(stmt_get_, 1, key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt_get_);
        if (rc != SQLITE_ROW) {
            if (rc == SQLITE_DONE) {
                return std::unexpected(CacheError::NotFound);
            }
            LOG_ERROR("SQLite get error: {} - {}", rc, sqlite3_errmsg(db_));
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        ThumbnailEntry entry;

        // Read columns: source_path, file_hash, width, height, original_width, original_height,
        //               source_mtime, cached_at, data_size, pixel_data
        const char* source_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_, 0));
        const char* file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_, 1));

        entry.metadata.source_path = source_path ? source_path : "";
        entry.metadata.file_hash = file_hash ? file_hash : "";
        entry.metadata.width = static_cast<uint32_t>(sqlite3_column_int(stmt_get_, 2));
        entry.metadata.height = static_cast<uint32_t>(sqlite3_column_int(stmt_get_, 3));
        entry.metadata.original_width = static_cast<uint32_t>(sqlite3_column_int(stmt_get_, 4));
        entry.metadata.original_height = static_cast<uint32_t>(sqlite3_column_int(stmt_get_, 5));

        int64_t source_mtime = sqlite3_column_int64(stmt_get_, 6);
        int64_t cached_at = sqlite3_column_int64(stmt_get_, 7);
        entry.metadata.source_mtime = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(source_mtime));
        entry.metadata.cached_at =
            std::chrono::system_clock::time_point(std::chrono::system_clock::duration(cached_at));

        entry.metadata.data_size = static_cast<uint64_t>(sqlite3_column_int64(stmt_get_, 8));

        // Read pixel data blob
        const void* blob_data = sqlite3_column_blob(stmt_get_, 9);
        int blob_size = sqlite3_column_bytes(stmt_get_, 9);

        if (blob_data && blob_size > 0) {
            std::vector<uint8_t> raw_data(static_cast<size_t>(blob_size));
            std::memcpy(raw_data.data(), blob_data, static_cast<size_t>(blob_size));

#ifdef NIVE_HAS_ZSTD
            // Decompress if data is zstd compressed
            auto decompressed = decompress_data(raw_data);
            if (!decompressed) {
                return std::unexpected(decompressed.error());
            }
            entry.data = std::move(*decompressed);
#else
            entry.data = std::move(raw_data);
#endif
        }

        return entry;
    }

    [[nodiscard]] std::expected<ThumbnailMetadata, CacheError> getMetadata(const std::string& key) {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        stmt_get_metadata_.reset();
        sqlite3_bind_text(stmt_get_metadata_, 1, key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt_get_metadata_);
        if (rc != SQLITE_ROW) {
            if (rc == SQLITE_DONE) {
                return std::unexpected(CacheError::NotFound);
            }
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        ThumbnailMetadata metadata;

        const char* source_path =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_metadata_, 0));
        const char* file_hash =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_metadata_, 1));

        metadata.source_path = source_path ? source_path : "";
        metadata.file_hash = file_hash ? file_hash : "";
        metadata.width = static_cast<uint32_t>(sqlite3_column_int(stmt_get_metadata_, 2));
        metadata.height = static_cast<uint32_t>(sqlite3_column_int(stmt_get_metadata_, 3));
        metadata.original_width = static_cast<uint32_t>(sqlite3_column_int(stmt_get_metadata_, 4));
        metadata.original_height = static_cast<uint32_t>(sqlite3_column_int(stmt_get_metadata_, 5));

        int64_t source_mtime = sqlite3_column_int64(stmt_get_metadata_, 6);
        int64_t cached_at = sqlite3_column_int64(stmt_get_metadata_, 7);
        metadata.source_mtime = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(source_mtime));
        metadata.cached_at =
            std::chrono::system_clock::time_point(std::chrono::system_clock::duration(cached_at));

        metadata.data_size = static_cast<uint64_t>(sqlite3_column_int64(stmt_get_metadata_, 8));

        return metadata;
    }

    [[nodiscard]] bool exists(const std::string& key) {
        std::lock_guard lock(db_mutex_);

        if (!ensureOpen())
            return false;

        stmt_exists_.reset();
        sqlite3_bind_text(stmt_exists_, 1, key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt_exists_);
        if (rc == SQLITE_ROW) {
            return sqlite3_column_int(stmt_exists_, 0) > 0;
        }
        return false;
    }

    [[nodiscard]] std::expected<void, CacheError> put(const ThumbnailEntry& entry) {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

#ifdef NIVE_HAS_ZSTD
        // Compress data if compression is enabled
        auto compressed = compress_data(entry.data, compression_level_);
        if (!compressed) {
            return std::unexpected(compressed.error());
        }
        const auto& data_to_store = *compressed;
#else
        const auto& data_to_store = entry.data;
#endif

        stmt_put_.reset();

        // Bind parameters
        // INSERT OR REPLACE INTO thumbnails (cache_key, source_path, file_hash, width, height,
        //                                    original_width, original_height, source_mtime,
        //                                    cached_at, data_size, pixel_data)
        std::string source_path_str = pathToUtf8(entry.metadata.source_path);

        sqlite3_bind_text(stmt_put_, 1, entry.metadata.file_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt_put_, 2, source_path_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt_put_, 3, entry.metadata.file_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt_put_, 4, static_cast<int>(entry.metadata.width));
        sqlite3_bind_int(stmt_put_, 5, static_cast<int>(entry.metadata.height));
        sqlite3_bind_int(stmt_put_, 6, static_cast<int>(entry.metadata.original_width));
        sqlite3_bind_int(stmt_put_, 7, static_cast<int>(entry.metadata.original_height));
        sqlite3_bind_int64(stmt_put_, 8, entry.metadata.source_mtime.time_since_epoch().count());
        sqlite3_bind_int64(stmt_put_, 9, entry.metadata.cached_at.time_since_epoch().count());
        sqlite3_bind_int64(stmt_put_, 10, static_cast<sqlite3_int64>(entry.data.size()));
        sqlite3_bind_blob(stmt_put_, 11, data_to_store.data(),
                          static_cast<int>(data_to_store.size()), SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt_put_);
        if (rc != SQLITE_DONE) {
            LOG_ERROR("SQLite put error: {} - {}", rc, sqlite3_errmsg(db_));
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        return {};
    }

    [[nodiscard]] std::expected<void, CacheError> remove(const std::string& key) {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        stmt_remove_.reset();
        sqlite3_bind_text(stmt_remove_, 1, key.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt_remove_);
        if (rc != SQLITE_DONE) {
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        return {};
    }

    [[nodiscard]] std::expected<uint64_t, CacheError>
    removeOlderThan(std::chrono::system_clock::time_point older_than) {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        stmt_remove_older_than_.reset();
        sqlite3_bind_int64(stmt_remove_older_than_, 1, older_than.time_since_epoch().count());

        int rc = sqlite3_step(stmt_remove_older_than_);
        if (rc != SQLITE_DONE) {
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        return static_cast<uint64_t>(sqlite3_changes(db_));
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> removeOrphaned() {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        // First, get all source paths
        std::vector<std::pair<std::string, std::string>>
            entries_to_check;  // cache_key, source_path

        stmt_get_all_paths_.reset();

        while (sqlite3_step(stmt_get_all_paths_) == SQLITE_ROW) {
            const char* cache_key =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_all_paths_, 0));
            const char* source_path =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_all_paths_, 1));
            if (cache_key && source_path) {
                entries_to_check.emplace_back(cache_key, source_path);
            }
        }

        // Check which files no longer exist and remove them
        uint64_t count = 0;
        for (const auto& [cache_key, source_path] : entries_to_check) {
            if (!std::filesystem::exists(source_path)) {
                stmt_remove_.reset();
                sqlite3_bind_text(stmt_remove_, 1, cache_key.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt_remove_) == SQLITE_DONE) {
                    ++count;
                }
            }
        }

        return count;
    }

    [[nodiscard]] std::expected<CacheStats, CacheError> getStats() {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        CacheStats stats;

        stmt_get_stats_.reset();

        int rc = sqlite3_step(stmt_get_stats_);
        if (rc == SQLITE_ROW) {
            stats.total_entries = static_cast<uint64_t>(sqlite3_column_int64(stmt_get_stats_, 0));
            stats.total_size_bytes =
                static_cast<uint64_t>(sqlite3_column_int64(stmt_get_stats_, 1));

            if (sqlite3_column_type(stmt_get_stats_, 2) != SQLITE_NULL) {
                int64_t oldest = sqlite3_column_int64(stmt_get_stats_, 2);
                stats.oldest_entry = std::chrono::system_clock::time_point(
                    std::chrono::system_clock::duration(oldest));
            }

            if (sqlite3_column_type(stmt_get_stats_, 3) != SQLITE_NULL) {
                int64_t newest = sqlite3_column_int64(stmt_get_stats_, 3);
                stats.newest_entry = std::chrono::system_clock::time_point(
                    std::chrono::system_clock::duration(newest));
            }
        }

        return stats;
    }

    [[nodiscard]] std::expected<uint64_t, CacheError> clear() {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        // First get the count
        stmt_count_.reset();
        uint64_t count = 0;
        if (sqlite3_step(stmt_count_) == SQLITE_ROW) {
            count = static_cast<uint64_t>(sqlite3_column_int64(stmt_count_, 0));
        }

        // Then clear
        stmt_clear_.reset();
        int rc = sqlite3_step(stmt_clear_);
        if (rc != SQLITE_DONE) {
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        return count;
    }

    [[nodiscard]] std::expected<void, CacheError> vacuum() {
        std::lock_guard lock(db_mutex_);

        if (auto guard = ensureOpen(); !guard)
            return std::unexpected(guard.error());

        int rc = sqlite3_exec(db_, "VACUUM;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            return std::unexpected(sqlite_to_cache_error(rc));
        }

        return {};
    }

    // Async operations

    void getAsync(const std::string& key, AsyncCallback<ThumbnailEntry> callback) {
        enqueue_task([this, key, callback = std::move(callback)]() { callback(get(key)); });
    }

    void putAsync(ThumbnailEntry entry, AsyncCallback<void> callback) {
        enqueue_task([this, entry = std::move(entry), callback = std::move(callback)]() {
            callback(put(entry));
        });
    }

    void removeAsync(const std::string& key, AsyncCallback<void> callback) {
        enqueue_task([this, key, callback = std::move(callback)]() { callback(remove(key)); });
    }

private:
    [[nodiscard]] std::expected<void, CacheError> ensureOpen() const {
        if (!db_) {
            return std::unexpected(CacheError::DatabaseError);
        }
        return {};
    }

    bool create_tables() {
        // Check schema version and migrate if needed
        if (!check_and_migrate_schema()) {
            return false;
        }

        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS thumbnails (
                cache_key TEXT PRIMARY KEY,
                source_path TEXT NOT NULL,
                file_hash TEXT NOT NULL,
                width INTEGER NOT NULL,
                height INTEGER NOT NULL,
                original_width INTEGER NOT NULL,
                original_height INTEGER NOT NULL,
                source_mtime INTEGER NOT NULL,
                cached_at INTEGER NOT NULL,
                data_size INTEGER NOT NULL,
                pixel_data BLOB NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_file_hash ON thumbnails(file_hash);
            CREATE INDEX IF NOT EXISTS idx_cached_at ON thumbnails(cached_at);
            CREATE INDEX IF NOT EXISTS idx_source_path ON thumbnails(source_path);
        )";

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to create tables: {}", err_msg ? err_msg : "unknown error");
            sqlite3_free(err_msg);
            return false;
        }

        return true;
    }

    bool check_and_migrate_schema() {
        // Create schema_version table if not exists
        const char* create_version_table = R"(
            CREATE TABLE IF NOT EXISTS schema_version (
                version INTEGER NOT NULL
            );
        )";

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, create_version_table, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to create schema_version table: {}",
                      err_msg ? err_msg : "unknown error");
            sqlite3_free(err_msg);
            return false;
        }

        // Get current schema version
        int current_version = 0;
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, "SELECT version FROM schema_version LIMIT 1;", -1, &stmt,
                                nullptr);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                current_version = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (current_version == CURRENT_SCHEMA_VERSION) {
            return true;  // No migration needed
        }

        LOG_INFO("Cache schema version {} -> {}, clearing old cache", current_version,
                 CURRENT_SCHEMA_VERSION);

        // Drop old thumbnails table if exists (cache invalidation)
        rc = sqlite3_exec(db_, "DROP TABLE IF EXISTS thumbnails;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to drop old thumbnails table: {}",
                      err_msg ? err_msg : "unknown error");
            sqlite3_free(err_msg);
            return false;
        }

        // Update or insert schema version
        std::string version_sql;
        if (current_version == 0) {
            version_sql = "INSERT INTO schema_version (version) VALUES (" +
                          std::to_string(CURRENT_SCHEMA_VERSION) + ");";
        } else {
            version_sql =
                "UPDATE schema_version SET version = " + std::to_string(CURRENT_SCHEMA_VERSION) +
                ";";
        }
        rc = sqlite3_exec(db_, version_sql.c_str(), nullptr, nullptr, &err_msg);

        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to update schema version: {}", err_msg ? err_msg : "unknown error");
            sqlite3_free(err_msg);
            return false;
        }

        return true;
    }

    bool prepare_statements() {
        // Get full entry
        if (!stmt_get_.prepare(
                db_,
                "SELECT source_path, file_hash, width, height, original_width, original_height, "
                "source_mtime, cached_at, data_size, pixel_data "
                "FROM thumbnails WHERE cache_key = ?;")) {
            return false;
        }

        // Get metadata only
        if (!stmt_get_metadata_.prepare(
                db_,
                "SELECT source_path, file_hash, width, height, original_width, original_height, "
                "source_mtime, cached_at, data_size "
                "FROM thumbnails WHERE cache_key = ?;")) {
            return false;
        }

        // Check exists
        if (!stmt_exists_.prepare(db_, "SELECT COUNT(*) FROM thumbnails WHERE cache_key = ?;")) {
            return false;
        }

        // Insert or replace
        if (!stmt_put_.prepare(db_,
                               "INSERT OR REPLACE INTO thumbnails "
                               "(cache_key, source_path, file_hash, width, height, original_width, "
                               "original_height, "
                               "source_mtime, cached_at, data_size, pixel_data) "
                               "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);")) {
            return false;
        }

        // Delete by key
        if (!stmt_remove_.prepare(db_, "DELETE FROM thumbnails WHERE cache_key = ?;")) {
            return false;
        }

        // Delete older than
        if (!stmt_remove_older_than_.prepare(db_, "DELETE FROM thumbnails WHERE cached_at < ?;")) {
            return false;
        }

        // Get all paths (for orphan detection)
        if (!stmt_get_all_paths_.prepare(db_, "SELECT cache_key, source_path FROM thumbnails;")) {
            return false;
        }

        // Get stats
        if (!stmt_get_stats_.prepare(
                db_,
                "SELECT COUNT(*), COALESCE(SUM(data_size), 0), MIN(cached_at), MAX(cached_at) "
                "FROM thumbnails;")) {
            return false;
        }

        // Clear all
        if (!stmt_clear_.prepare(db_, "DELETE FROM thumbnails;")) {
            return false;
        }

        // Count entries
        if (!stmt_count_.prepare(db_, "SELECT COUNT(*) FROM thumbnails;")) {
            return false;
        }

        return true;
    }

    void enqueue_task(std::function<void()> task) {
        {
            std::lock_guard lock(queue_mutex_);
            task_queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void worker_thread(std::stop_token stop_token) {
        while (!stop_token.stop_requested() && running_) {
            std::function<void()> task;

            {
                std::unique_lock lock(queue_mutex_);
                cv_.wait(lock, [this, &stop_token] {
                    return !task_queue_.empty() || !running_ || stop_token.stop_requested();
                });

                if (stop_token.stop_requested() || !running_) {
                    break;
                }

                if (!task_queue_.empty()) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop();
                }
            }

            if (task) {
                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR("Cache async task failed: {}", e.what());
                } catch (...) {
                    LOG_ERROR("Cache async task failed with unknown exception");
                }
            }
        }
    }

    std::filesystem::path db_path_;
    int compression_level_ = 3;
    sqlite3* db_ = nullptr;

    // Prepared statements
    SqliteStatement stmt_get_;
    SqliteStatement stmt_get_metadata_;
    SqliteStatement stmt_exists_;
    SqliteStatement stmt_put_;
    SqliteStatement stmt_remove_;
    SqliteStatement stmt_remove_older_than_;
    SqliteStatement stmt_get_all_paths_;
    SqliteStatement stmt_get_stats_;
    SqliteStatement stmt_clear_;
    SqliteStatement stmt_count_;

    // Mutex for database access
    std::mutex db_mutex_;

    // Async task queue
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::jthread worker_;
    std::atomic<bool> running_;
};

// Public interface

std::expected<std::unique_ptr<CacheDatabase>, CacheError>
CacheDatabase::open(const std::filesystem::path& path, int compression_level) {
    // If path is a directory, append the database filename
    std::filesystem::path db_path = path;
    if (std::filesystem::is_directory(path) || !path.has_extension()) {
        db_path = path / "thumbnails.db";
    }

    auto db = std::unique_ptr<CacheDatabase>(new CacheDatabase());
    db->impl_ = std::make_unique<Impl>(db_path, compression_level);

    if (!db->impl_->initialize()) {
        return std::unexpected(CacheError::DatabaseError);
    }

    return db;
}

CacheDatabase::CacheDatabase() = default;
CacheDatabase::~CacheDatabase() = default;

bool CacheDatabase::isOpen() const noexcept {
    return impl_ && impl_->isOpen();
}

const std::filesystem::path& CacheDatabase::path() const noexcept {
    static std::filesystem::path empty;
    return impl_ ? impl_->path() : empty;
}

std::expected<ThumbnailEntry, CacheError> CacheDatabase::get(const std::string& key) {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->get(key);
}

std::expected<ThumbnailMetadata, CacheError> CacheDatabase::getMetadata(const std::string& key) {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->getMetadata(key);
}

bool CacheDatabase::exists(const std::string& key) {
    return impl_ && impl_->exists(key);
}

std::expected<void, CacheError> CacheDatabase::put(const ThumbnailEntry& entry) {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->put(entry);
}

std::expected<void, CacheError> CacheDatabase::remove(const std::string& key) {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->remove(key);
}

std::expected<uint64_t, CacheError>
CacheDatabase::removeOlderThan(std::chrono::system_clock::time_point older_than) {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->removeOlderThan(older_than);
}

std::expected<uint64_t, CacheError> CacheDatabase::removeOrphaned() {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->removeOrphaned();
}

std::expected<CacheStats, CacheError> CacheDatabase::getStats() {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->getStats();
}

std::expected<uint64_t, CacheError> CacheDatabase::clear() {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->clear();
}

std::expected<void, CacheError> CacheDatabase::vacuum() {
    if (!impl_)
        return std::unexpected(CacheError::DatabaseError);
    return impl_->vacuum();
}

void CacheDatabase::getAsync(const std::string& key, AsyncCallback<ThumbnailEntry> callback) {
    if (!impl_) {
        callback(std::unexpected(CacheError::DatabaseError));
        return;
    }
    impl_->getAsync(key, std::move(callback));
}

void CacheDatabase::putAsync(ThumbnailEntry entry, AsyncCallback<void> callback) {
    if (!impl_) {
        callback(std::unexpected(CacheError::DatabaseError));
        return;
    }
    impl_->putAsync(std::move(entry), std::move(callback));
}

void CacheDatabase::removeAsync(const std::string& key, AsyncCallback<void> callback) {
    if (!impl_) {
        callback(std::unexpected(CacheError::DatabaseError));
        return;
    }
    impl_->removeAsync(key, std::move(callback));
}

}  // namespace nive::cache
