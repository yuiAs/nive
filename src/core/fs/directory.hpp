/// @file directory.hpp
/// @brief Directory scanning and file listing

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "file_metadata.hpp"

namespace nive::fs {

/// @brief Directory scan error types
enum class DirectoryError {
    Success,
    NotFound,
    AccessDenied,
    NotADirectory,
    IoError
};

/// @brief Convert error to string
[[nodiscard]] constexpr std::string_view to_string(DirectoryError error) noexcept {
    switch (error) {
    case DirectoryError::Success:
        return "Success";
    case DirectoryError::NotFound:
        return "Not found";
    case DirectoryError::AccessDenied:
        return "Access denied";
    case DirectoryError::NotADirectory:
        return "Not a directory";
    case DirectoryError::IoError:
        return "I/O error";
    default:
        return "Unknown error";
    }
}

/// @brief Sort order for directory listings
enum class SortOrder {
    Natural,      // Natural sort (default)
    NaturalDesc,  // Natural sort descending
    Name,         // Alphabetical
    NameDesc,
    Size,  // By file size
    SizeDesc,
    Modified,  // By modification time
    ModifiedDesc,
    Type,  // By file type/extension
    TypeDesc
};

/// @brief Filter options for directory scanning
struct DirectoryFilter {
    bool include_hidden = false;
    bool include_system = false;
    bool directories_only = false;
    bool files_only = false;
    bool images_only = false;
    bool archives_only = false;

    std::vector<std::wstring> extensions;  // Empty = all extensions
};

/// @brief Directory listing result
struct DirectoryListing {
    std::filesystem::path path;
    std::vector<FileMetadata> entries;
    uint64_t total_files = 0;
    uint64_t total_directories = 0;
    uint64_t total_size_bytes = 0;
};

/// @brief Scan directory and return file listing
/// @param path Directory path
/// @param filter Filter options
/// @param sort_order Sort order
/// @return Directory listing or error
[[nodiscard]] std::expected<DirectoryListing, DirectoryError>
scanDirectory(const std::filesystem::path& path, const DirectoryFilter& filter = {},
              SortOrder sort_order = SortOrder::Natural);

/// @brief Scan directory asynchronously
/// @param path Directory path
/// @param filter Filter options
/// @param sort_order Sort order
/// @param callback Called with result
/// @param progress_callback Called periodically with progress (optional)
void scanDirectoryAsync(
    const std::filesystem::path& path, const DirectoryFilter& filter, SortOrder sort_order,
    std::function<void(std::expected<DirectoryListing, DirectoryError>)> callback,
    std::function<void(size_t current, size_t total)> progress_callback = nullptr);

/// @brief Get subdirectories of a directory
/// @param path Directory path
/// @param include_hidden Include hidden directories
/// @return List of subdirectory paths or error
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, DirectoryError>
getSubdirectories(const std::filesystem::path& path, bool include_hidden = false);

/// @brief Get parent directory path
/// @param path Current path
/// @return Parent path or nullopt if at root
[[nodiscard]] std::optional<std::filesystem::path>
getParentDirectory(const std::filesystem::path& path);

/// @brief Check if directory is empty
[[nodiscard]] bool isDirectoryEmpty(const std::filesystem::path& path);

/// @brief Count files in directory (non-recursive)
[[nodiscard]] std::expected<size_t, DirectoryError> countFiles(const std::filesystem::path& path,
                                                               const DirectoryFilter& filter = {});

/// @brief Get list of drive letters on the system
[[nodiscard]] std::vector<std::filesystem::path> getDrives();

/// @brief Get special folder path (Desktop, Documents, Pictures, etc.)
/// @param folder_id CSIDL constant (e.g., CSIDL_DESKTOP)
[[nodiscard]] std::optional<std::filesystem::path> getSpecialFolder(int folder_id);

}  // namespace nive::fs
