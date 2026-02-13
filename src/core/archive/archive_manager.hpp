/// @file archive_manager.hpp
/// @brief High-level archive management

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "archive_entry.hpp"
#include "archive_error.hpp"
#include "archive_reader.hpp"
#include "virtual_path.hpp"

namespace nive::archive {

/// @brief Callback for password request
/// @param archive_path Path to the archive
/// @return Password, or nullopt to cancel
using PasswordCallback =
    std::function<std::optional<std::wstring>(const std::filesystem::path& archive_path)>;

/// @brief Archive manager configuration
struct ArchiveManagerConfig {
    // Maximum number of cached archive readers
    size_t max_cached_archives = 4;

    // Password callback for encrypted archives
    PasswordCallback password_callback = nullptr;

    // Temporary directory for extracted files
    std::filesystem::path temp_dir;
};

/// @brief High-level archive manager
///
/// Provides convenient access to archive contents with caching.
/// Thread-safe for concurrent access.
class ArchiveManager {
public:
    /// @brief Create archive manager
    /// @param config Configuration
    explicit ArchiveManager(ArchiveManagerConfig config = {});
    ~ArchiveManager();

    // Non-copyable
    ArchiveManager(const ArchiveManager&) = delete;
    ArchiveManager& operator=(const ArchiveManager&) = delete;

    /// @brief Check if archive support is available
    [[nodiscard]] bool isAvailable() const noexcept;

    /// @brief Get path to 7z.dll
    [[nodiscard]] std::optional<std::filesystem::path> getDllPath() const noexcept;

    /// @brief Open an archive and get its info
    /// @param path Archive file path
    /// @return Archive info or error
    [[nodiscard]] std::expected<ArchiveInfo, ArchiveError> open(const std::filesystem::path& path);

    /// @brief Check if a file is a supported archive
    [[nodiscard]] bool isArchive(const std::filesystem::path& path) const noexcept;

    /// @brief Get entries in an archive
    /// @param archive_path Archive file path
    /// @return List of entries or error
    [[nodiscard]] std::expected<std::vector<ArchiveEntry>, ArchiveError>
    listEntries(const std::filesystem::path& archive_path);

    /// @brief Get image entries in an archive (sorted naturally)
    /// @param archive_path Archive file path
    /// @return List of image entries or error
    [[nodiscard]] std::expected<std::vector<ArchiveEntry>, ArchiveError>
    getImageEntries(const std::filesystem::path& archive_path);

    /// @brief Extract a file from archive to memory
    /// @param virtual_path Virtual path (archive|internal_path)
    /// @return File contents or error
    [[nodiscard]] std::expected<std::vector<uint8_t>, ArchiveError>
    extractToMemory(const VirtualPath& virtual_path);

    /// @brief Extract a file from archive to temporary file
    /// @param virtual_path Virtual path
    /// @return Path to temporary file or error
    ///
    /// The temporary file is managed by ArchiveManager and will be
    /// cleaned up when the manager is destroyed or cache is cleared.
    [[nodiscard]] std::expected<std::filesystem::path, ArchiveError>
    extractToTemp(const VirtualPath& virtual_path);

    /// @brief Extract a file from archive to specific location
    /// @param virtual_path Virtual path
    /// @param dest_path Destination path
    /// @param progress Progress callback
    /// @return Success or error
    [[nodiscard]] std::expected<void, ArchiveError>
    extractToFile(const VirtualPath& virtual_path, const std::filesystem::path& dest_path,
                  ExtractProgressCallback progress = nullptr);

    /// @brief Extract entire archive to directory
    /// @param archive_path Archive file path
    /// @param dest_dir Destination directory
    /// @param progress Progress callback
    /// @return Success or error
    [[nodiscard]] std::expected<void, ArchiveError>
    extractAll(const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir,
               ExtractProgressCallback progress = nullptr);

    /// @brief Clear cached archive readers
    void clearCache();

    /// @brief Clean up temporary extracted files
    void cleanupTempFiles();

    /// @brief Set password callback
    void setPasswordCallback(PasswordCallback callback);

private:
    /// @brief Get or create reader for archive
    [[nodiscard]] std::expected<IArchiveReader*, ArchiveError>
    getReader(const std::filesystem::path& archive_path);

    /// @brief Try to open archive with password retry
    [[nodiscard]] std::expected<void, ArchiveError>
    openWithPasswordRetry(IArchiveReader* reader, const std::filesystem::path& archive_path);

    struct CachedReader {
        std::unique_ptr<IArchiveReader> reader;
        std::filesystem::path path;
        std::chrono::steady_clock::time_point last_access;
    };

    ArchiveManagerConfig config_;
    std::vector<CachedReader> cache_;
    std::vector<std::filesystem::path> temp_files_;
    mutable std::mutex mutex_;
};

}  // namespace nive::archive
