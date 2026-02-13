/// @file trash.hpp
/// @brief Recycle bin operations using Windows Shell API

#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nive::fs {

/// @brief Trash operation error types
enum class TrashError {
    Success,
    NotFound,
    AccessDenied,
    Cancelled,
    IoError
};

/// @brief Convert error to string
[[nodiscard]] constexpr std::string_view to_string(TrashError error) noexcept {
    switch (error) {
    case TrashError::Success:
        return "Success";
    case TrashError::NotFound:
        return "Not found";
    case TrashError::AccessDenied:
        return "Access denied";
    case TrashError::Cancelled:
        return "Cancelled";
    case TrashError::IoError:
        return "I/O error";
    default:
        return "Unknown error";
    }
}

/// @brief Options for trash operations
struct TrashOptions {
    bool show_confirmation = false;  // Show confirmation dialog
    bool show_progress = false;      // Show progress dialog
    bool allow_undo = true;          // Allow undo (recycle bin)
    bool silent = true;              // No UI at all
};

/// @brief Result of trash operation
struct TrashResult {
    TrashError error = TrashError::Success;
    uint64_t files_trashed = 0;
    std::vector<std::filesystem::path> failed_files;

    [[nodiscard]] bool succeeded() const noexcept { return error == TrashError::Success; }
};

/// @brief Move a single file to recycle bin
/// @param path File path
/// @param options Trash options
/// @return Success or error
[[nodiscard]] std::expected<void, TrashError> trashFile(const std::filesystem::path& path,
                                                        const TrashOptions& options = {});

/// @brief Move multiple files to recycle bin
/// @param paths File paths
/// @param options Trash options
/// @return Operation result
[[nodiscard]] TrashResult trashFiles(std::span<const std::filesystem::path> paths,
                                     const TrashOptions& options = {});

/// @brief Permanently delete a file (bypass recycle bin)
/// @param path File path
/// @param options Options (show_confirmation applies)
/// @return Success or error
[[nodiscard]] std::expected<void, TrashError> permanentDelete(const std::filesystem::path& path,
                                                              const TrashOptions& options = {});

/// @brief Permanently delete multiple files
/// @param paths File paths
/// @param options Options
/// @return Operation result
[[nodiscard]] TrashResult permanentDeleteFiles(std::span<const std::filesystem::path> paths,
                                               const TrashOptions& options = {});

/// @brief Empty the recycle bin
/// @param drive_letter Optional drive letter (empty for all drives)
/// @param show_confirmation Show confirmation dialog
/// @param show_progress Show progress dialog
/// @return Success or error
[[nodiscard]] std::expected<void, TrashError>
emptyRecycleBin(const std::wstring& drive_letter = L"", bool show_confirmation = true,
                bool show_progress = true);

/// @brief Get recycle bin item count and size
/// @param drive_letter Optional drive letter (empty for all drives)
/// @return Pair of (item count, total size in bytes)
[[nodiscard]] std::expected<std::pair<uint64_t, uint64_t>, TrashError>
getRecycleBinInfo(const std::wstring& drive_letter = L"");

}  // namespace nive::fs
