/// @file file_operation_manager.hpp
/// @brief Coordinator for file operations with conflict handling and UI

#pragma once

#include <Windows.h>

#include <oleidl.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "core/fs/file_conflict.hpp"
#include "core/fs/file_operations.hpp"
#include "core/fs/shell_file_operation.hpp"

namespace nive::ui {

/// @brief Callback when file operation completes
/// @param success True if operation completed (possibly with some failures)
/// @param result Detailed operation result
using FileOperationCompleteCallback =
    std::function<void(bool success, const fs::FileOperationResult& result)>;

/// @brief Options for file operations
struct FileOperationOptions {
    bool show_conflict_dialog = true;  // Show dialog on conflicts
    bool show_delete_confirm = true;   // Show confirmation for delete
    bool default_to_trash = true;      // Default delete to trash
    bool show_progress = false;        // Show progress dialog (future)
};

/// @brief Manages file operations with UI feedback
///
/// Coordinates between file system operations, conflict detection,
/// and UI dialogs for user interaction. Uses IFileOperation COM API
/// for OS-native progress dialog, undo support, and Shell integration.
class FileOperationManager {
public:
    explicit FileOperationManager(HWND parent_window);
    ~FileOperationManager() = default;

    FileOperationManager(const FileOperationManager&) = delete;
    FileOperationManager& operator=(const FileOperationManager&) = delete;

    /// @brief Copy files to a destination directory
    /// @param files Source file paths
    /// @param dest_dir Destination directory
    /// @param options Operation options
    /// @return Operation result
    fs::FileOperationResult copyFiles(const std::vector<std::filesystem::path>& files,
                                      const std::filesystem::path& dest_dir,
                                      const FileOperationOptions& options = {});

    /// @brief Move files to a destination directory
    /// @param files Source file paths
    /// @param dest_dir Destination directory
    /// @param options Operation options
    /// @return Operation result
    fs::FileOperationResult moveFiles(const std::vector<std::filesystem::path>& files,
                                      const std::filesystem::path& dest_dir,
                                      const FileOperationOptions& options = {});

    /// @brief Delete files (with confirmation)
    /// @param files Files to delete
    /// @param options Operation options
    /// @return Operation result
    fs::FileOperationResult deleteFiles(const std::vector<std::filesystem::path>& files,
                                        const FileOperationOptions& options = {});

    /// @brief Handle a drop operation from D&D
    /// @param files Dropped file paths
    /// @param dest_dir Destination directory
    /// @param effect Drop effect (DROPEFFECT_COPY or DROPEFFECT_MOVE)
    /// @param options Operation options
    /// @return Operation result
    fs::FileOperationResult handleDrop(const std::vector<std::filesystem::path>& files,
                                       const std::filesystem::path& dest_dir, DWORD effect,
                                       const FileOperationOptions& options = {});

    /// @brief Set callback for operation completion
    void onOperationComplete(FileOperationCompleteCallback callback) {
        complete_callback_ = std::move(callback);
    }

private:
    /// @brief Resolve conflicts and build items list for copy/move operations
    /// @param files Source files
    /// @param dest_dir Destination directory
    /// @param show_dialog Whether to show conflict dialog
    /// @return Resolved items list, or nullopt if operation was cancelled
    std::optional<std::vector<fs::ResolvedFileItem>>
    resolveAllConflicts(const std::vector<std::filesystem::path>& files,
                        const std::filesystem::path& dest_dir, bool show_dialog);

    /// @brief Resolve a single conflict according to resolution
    /// @param conflict Conflict info
    /// @param resolution User's resolution choice
    /// @param move_to_trash Whether to move replaced file to trash
    /// @return Resolved item, or nullopt to skip this file
    std::optional<fs::ResolvedFileItem> resolveConflict(const fs::FileConflictInfo& conflict,
                                                        const fs::ConflictResolution& resolution,
                                                        bool move_to_trash);

    /// @brief Show conflict dialog and get resolution
    std::optional<fs::ConflictResolution> showConflictDialog(const fs::FileConflictInfo& conflict);

    /// @brief Show error dialog for failed operations
    void showErrorDialog(const fs::FileOperationResult& result, const std::wstring& operation);

    HWND parent_window_;
    FileOperationCompleteCallback complete_callback_;

    // State for "apply to all" during batch operations
    std::optional<fs::ConflictResolution> apply_to_all_resolution_;
};

}  // namespace nive::ui
