/// @file file_conflict.hpp
/// @brief File conflict detection and resolution structures

#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace nive::fs {

/// @brief Action to take when a file conflict is detected
enum class ConflictAction {
    Overwrite,   // Replace existing file
    Skip,        // Skip this file
    Rename,      // Use custom name
    AutoNumber,  // Add number suffix (e.g., "file (1).txt")
    KeepNewer,   // Keep the file with newer modification time
    KeepLarger,  // Keep the larger file
};

/// @brief Information about a file conflict
struct FileConflictInfo {
    std::filesystem::path source_path;
    std::filesystem::path dest_path;
    uint64_t source_size = 0;
    uint64_t dest_size = 0;
    std::filesystem::file_time_type source_time;
    std::filesystem::file_time_type dest_time;
    bool files_identical = false;  // Same size + quick hash check
};

/// @brief User's resolution for a conflict
struct ConflictResolution {
    ConflictAction action = ConflictAction::Skip;
    std::wstring custom_name;             // Used when action == Rename
    bool apply_to_all = false;            // Apply this resolution to all conflicts
    bool move_replaced_to_trash = false;  // Move replaced file to recycle bin
    bool skip_identical = false;          // Skip files that are identical
};

/// @brief Validation result for file operations
struct OperationValidation {
    bool valid = true;
    std::wstring error_message;

    // Specific validation flags
    bool source_exists = true;
    bool dest_writable = true;
    bool no_circular_copy = true;
    bool sufficient_space = true;

    [[nodiscard]] explicit operator bool() const noexcept { return valid; }
};

/// @brief Validate a file operation before execution
/// @param source Source file path
/// @param dest Destination path
/// @param is_move True if moving, false if copying
/// @return Validation result
[[nodiscard]] OperationValidation validateOperation(const std::filesystem::path& source,
                                                    const std::filesystem::path& dest,
                                                    bool is_move);

/// @brief Check if two files are identical (quick check using size and partial hash)
/// @param path1 First file path
/// @param path2 Second file path
/// @return True if files appear to be identical
[[nodiscard]] bool areFilesIdentical(const std::filesystem::path& path1,
                                     const std::filesystem::path& path2);

/// @brief Detect conflicts for a batch of file operations
/// @param sources Source file paths
/// @param dest_dir Destination directory
/// @return List of conflicts found
[[nodiscard]] std::vector<FileConflictInfo>
detectConflicts(const std::vector<std::filesystem::path>& sources,
                const std::filesystem::path& dest_dir);

/// @brief Get available disk space on the drive containing the path
/// @param path Path to check
/// @return Available space in bytes, or 0 on error
[[nodiscard]] uint64_t getAvailableSpace(const std::filesystem::path& path);

/// @brief Calculate total size of files
/// @param paths File paths
/// @return Total size in bytes
[[nodiscard]] uint64_t calculateTotalSize(const std::vector<std::filesystem::path>& paths);

/// @brief Check if dest is a subdirectory of source (circular copy detection)
/// @param source Source path
/// @param dest Destination path
/// @return True if dest is inside source
[[nodiscard]] bool isSubdirectory(const std::filesystem::path& source,
                                  const std::filesystem::path& dest);

}  // namespace nive::fs
