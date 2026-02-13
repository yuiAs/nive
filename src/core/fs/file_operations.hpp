/// @file file_operations.hpp
/// @brief File operations (copy, move, delete)

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "file_conflict.hpp"

namespace nive::fs {

/// @brief File operation error types
enum class FileOperationError {
    NotFound,
    AccessDenied,
    AlreadyExists,
    DiskFull,
    PathTooLong,
    InvalidPath,
    IoError,
    Cancelled,
    PartialSuccess
};

/// @brief Convert error to string
[[nodiscard]] constexpr std::string_view to_string(FileOperationError error) noexcept {
    switch (error) {
    case FileOperationError::NotFound:
        return "Not found";
    case FileOperationError::AccessDenied:
        return "Access denied";
    case FileOperationError::AlreadyExists:
        return "Already exists";
    case FileOperationError::DiskFull:
        return "Disk full";
    case FileOperationError::PathTooLong:
        return "Path too long";
    case FileOperationError::InvalidPath:
        return "Invalid path";
    case FileOperationError::IoError:
        return "I/O error";
    case FileOperationError::Cancelled:
        return "Cancelled";
    case FileOperationError::PartialSuccess:
        return "Partial success";
    default:
        return "Unknown error";
    }
}

/// @brief Options for copy/move operations
struct CopyOptions {
    bool overwrite_existing = false;
    bool skip_existing = false;
    bool recursive = true;
    bool preserve_timestamps = true;
};

/// @brief Progress callback for file operations
/// @param current_file Current file being processed
/// @param current Current bytes processed
/// @param total Total bytes to process
/// @return false to cancel operation
using ProgressCallback = std::function<bool(const std::filesystem::path& current_file,
                                            uint64_t current, uint64_t total)>;

/// @brief Result of file operation
struct FileOperationResult {
    std::optional<FileOperationError> error;
    uint64_t files_processed = 0;
    uint64_t bytes_processed = 0;
    std::vector<std::filesystem::path> failed_files;

    [[nodiscard]] bool succeeded() const noexcept { return !error.has_value(); }

    [[nodiscard]] bool partiallySucceeded() const noexcept {
        return error == FileOperationError::PartialSuccess;
    }
};

/// @brief Copy a single file
/// @param source Source file path
/// @param dest Destination file path
/// @param options Copy options
/// @return Success or error
[[nodiscard]] std::expected<void, FileOperationError> copyFile(const std::filesystem::path& source,
                                                               const std::filesystem::path& dest,
                                                               const CopyOptions& options = {});

/// @brief Copy multiple files
/// @param sources Source file paths
/// @param dest_dir Destination directory
/// @param options Copy options
/// @param progress Progress callback (optional)
/// @return Operation result
[[nodiscard]] FileOperationResult copyFiles(std::span<const std::filesystem::path> sources,
                                            const std::filesystem::path& dest_dir,
                                            const CopyOptions& options = {},
                                            ProgressCallback progress = nullptr);

/// @brief Move a single file
/// @param source Source file path
/// @param dest Destination file path
/// @param options Move options
/// @return Success or error
[[nodiscard]] std::expected<void, FileOperationError> moveFile(const std::filesystem::path& source,
                                                               const std::filesystem::path& dest,
                                                               const CopyOptions& options = {});

/// @brief Move multiple files
/// @param sources Source file paths
/// @param dest_dir Destination directory
/// @param options Move options
/// @param progress Progress callback (optional)
/// @return Operation result
[[nodiscard]] FileOperationResult moveFiles(std::span<const std::filesystem::path> sources,
                                            const std::filesystem::path& dest_dir,
                                            const CopyOptions& options = {},
                                            ProgressCallback progress = nullptr);

/// @brief Delete a single file or empty directory
/// @param path File or directory path
/// @return Success or error
[[nodiscard]] std::expected<void, FileOperationError> deleteFile(const std::filesystem::path& path);

/// @brief Delete multiple files
/// @param paths File paths to delete
/// @param progress Progress callback (optional)
/// @return Operation result
[[nodiscard]] FileOperationResult deleteFiles(std::span<const std::filesystem::path> paths,
                                              ProgressCallback progress = nullptr);

/// @brief Delete directory recursively
/// @param path Directory path
/// @return Number of files deleted or error
[[nodiscard]] std::expected<uint64_t, FileOperationError>
deleteDirectory(const std::filesystem::path& path);

/// @brief Rename a file or directory
/// @param path Current path
/// @param new_name New name (not full path)
/// @return New path or error
[[nodiscard]] std::expected<std::filesystem::path, FileOperationError>
renameFile(const std::filesystem::path& path, const std::wstring& new_name);

/// @brief Create directory (and parents if needed)
/// @param path Directory path
/// @return Success or error
[[nodiscard]] std::expected<void, FileOperationError>
createDirectory(const std::filesystem::path& path);

/// @brief Check if path is valid (no invalid characters, not too long)
[[nodiscard]] bool isValidPath(const std::filesystem::path& path) noexcept;

/// @brief Check if filename is valid
[[nodiscard]] bool isValidFilename(std::wstring_view name) noexcept;

/// @brief Generate unique filename if file already exists
/// @param path Original path
/// @return Unique path (with number suffix if needed)
[[nodiscard]] std::filesystem::path generateUniquePath(const std::filesystem::path& path);

/// @brief Callback for conflict resolution
/// @param conflict Information about the conflict
/// @return Resolution chosen by user, or nullopt to cancel
using ConflictCallback =
    std::function<std::optional<ConflictResolution>(const FileConflictInfo& conflict)>;

/// @brief Extended options for copy/move operations with conflict handling
struct ExtendedCopyOptions : public CopyOptions {
    ConflictCallback on_conflict;         // Called when conflict detected
    bool move_replaced_to_trash = false;  // Move replaced files to recycle bin
    bool skip_identical = false;          // Skip files that are identical
};

/// @brief Copy files with conflict detection and resolution
/// @param sources Source file paths
/// @param dest_dir Destination directory
/// @param options Extended copy options with conflict callback
/// @param progress Progress callback (optional)
/// @return Operation result
[[nodiscard]] FileOperationResult copyFilesWithConflictHandling(
    std::span<const std::filesystem::path> sources, const std::filesystem::path& dest_dir,
    const ExtendedCopyOptions& options, ProgressCallback progress = nullptr);

/// @brief Move files with conflict detection and resolution
/// @param sources Source file paths
/// @param dest_dir Destination directory
/// @param options Extended copy options with conflict callback
/// @param progress Progress callback (optional)
/// @return Operation result
[[nodiscard]] FileOperationResult moveFilesWithConflictHandling(
    std::span<const std::filesystem::path> sources, const std::filesystem::path& dest_dir,
    const ExtendedCopyOptions& options, ProgressCallback progress = nullptr);

/// @brief Safely copy a single file with atomic write
///
/// Copies to a temporary file first, then renames to final destination.
/// This ensures the destination is never left in an inconsistent state.
///
/// @param source Source file path
/// @param dest Destination file path
/// @param options Copy options
/// @return Success or error
[[nodiscard]] std::expected<void, FileOperationError>
safeCopyFile(const std::filesystem::path& source, const std::filesystem::path& dest,
             const CopyOptions& options = {});

}  // namespace nive::fs
