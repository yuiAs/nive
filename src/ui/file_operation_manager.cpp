/// @file file_operation_manager.cpp
/// @brief File operation manager implementation using IFileOperation COM API

#include "file_operation_manager.hpp"

#include <format>

#include "core/fs/trash.hpp"
#include "core/i18n/i18n.hpp"
#include "ui/d2d/dialog/delete_confirm/d2d_delete_confirm_dialog.hpp"
#include "ui/d2d/dialog/file_conflict/d2d_file_conflict_dialog.hpp"

namespace nive::ui {

FileOperationManager::FileOperationManager(HWND parent_window) : parent_window_(parent_window) {
}

// ============================================================================
// Public API
// ============================================================================

fs::FileOperationResult
FileOperationManager::copyFiles(const std::vector<std::filesystem::path>& files,
                                const std::filesystem::path& dest_dir,
                                const FileOperationOptions& options) {
    apply_to_all_resolution_.reset();

    auto resolved = resolveAllConflicts(files, dest_dir, options.show_conflict_dialog);
    if (!resolved) {
        // User cancelled via conflict dialog
        fs::FileOperationResult result;
        result.error = fs::FileOperationError::Cancelled;
        if (complete_callback_) {
            complete_callback_(false, result);
        }
        return result;
    }

    auto result = fs::ShellFileOperation::copyItems(
        *resolved, {.owner_window = parent_window_, .allow_undo = true});

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, std::wstring(i18n::tr("file_operation.copy")));
    }

    if (complete_callback_) {
        complete_callback_(result.succeeded() || result.partiallySucceeded(), result);
    }

    return result;
}

fs::FileOperationResult
FileOperationManager::moveFiles(const std::vector<std::filesystem::path>& files,
                                const std::filesystem::path& dest_dir,
                                const FileOperationOptions& options) {
    apply_to_all_resolution_.reset();

    auto resolved = resolveAllConflicts(files, dest_dir, options.show_conflict_dialog);
    if (!resolved) {
        fs::FileOperationResult result;
        result.error = fs::FileOperationError::Cancelled;
        if (complete_callback_) {
            complete_callback_(false, result);
        }
        return result;
    }

    auto result = fs::ShellFileOperation::moveItems(
        *resolved, {.owner_window = parent_window_, .allow_undo = true});

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, std::wstring(i18n::tr("file_operation.move")));
    }

    if (complete_callback_) {
        complete_callback_(result.succeeded() || result.partiallySucceeded(), result);
    }

    return result;
}

fs::FileOperationResult
FileOperationManager::deleteFiles(const std::vector<std::filesystem::path>& files,
                                  const FileOperationOptions& options) {
    fs::FileOperationResult result;

    if (files.empty()) {
        return result;
    }

    bool use_trash = options.default_to_trash;

    // Show confirmation dialog
    if (options.show_delete_confirm) {
        DeleteConfirmOptions dialog_options;
        dialog_options.show_trash_option = true;
        dialog_options.default_to_trash = options.default_to_trash;

        auto confirm_result =
            d2d::showD2DDeleteConfirmDialog(parent_window_, files, dialog_options);

        if (confirm_result == DeleteConfirmResult::Cancel) {
            result.error = fs::FileOperationError::Cancelled;
            return result;
        }

        use_trash = (confirm_result == DeleteConfirmResult::Trash);
    }

    fs::ShellOperationOptions shell_options{.owner_window = parent_window_};

    if (use_trash) {
        shell_options.allow_undo = true;
        result = fs::ShellFileOperation::recycleItems(files, shell_options);
    } else {
        result = fs::ShellFileOperation::deleteItems(files, shell_options);
    }

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, std::wstring(i18n::tr("file_operation.delete")));
    }

    if (complete_callback_) {
        complete_callback_(result.succeeded() || result.partiallySucceeded(), result);
    }

    return result;
}

fs::FileOperationResult
FileOperationManager::handleDrop(const std::vector<std::filesystem::path>& files,
                                 const std::filesystem::path& dest_dir, DWORD effect,
                                 const FileOperationOptions& options) {
    if (effect == DROPEFFECT_MOVE) {
        return moveFiles(files, dest_dir, options);
    } else {
        return copyFiles(files, dest_dir, options);
    }
}

// ============================================================================
// Conflict resolution
// ============================================================================

std::optional<std::vector<fs::ResolvedFileItem>>
FileOperationManager::resolveAllConflicts(const std::vector<std::filesystem::path>& files,
                                          const std::filesystem::path& dest_dir,
                                          bool show_dialog) {
    auto conflicts = fs::detectConflicts(files, dest_dir);

    // Build a map of source path -> conflict info for quick lookup
    std::unordered_map<std::wstring, const fs::FileConflictInfo*> conflict_map;
    for (const auto& conflict : conflicts) {
        conflict_map[conflict.source_path.wstring()] = &conflict;
    }

    std::vector<fs::ResolvedFileItem> resolved_items;
    resolved_items.reserve(files.size());

    for (const auto& file : files) {
        auto it = conflict_map.find(file.wstring());
        if (it == conflict_map.end()) {
            // No conflict — add directly
            resolved_items.push_back({.source_path = file, .dest_dir = dest_dir});
            continue;
        }

        const auto& conflict = *it->second;

        // Handle skip_identical from apply-to-all
        if (apply_to_all_resolution_ && apply_to_all_resolution_->skip_identical &&
            conflict.files_identical) {
            continue;  // Skip identical file
        }

        // Determine resolution
        fs::ConflictResolution resolution;
        if (apply_to_all_resolution_) {
            resolution = *apply_to_all_resolution_;
        } else if (show_dialog) {
            auto maybe_resolution = showConflictDialog(conflict);
            if (!maybe_resolution) {
                return std::nullopt;  // User cancelled
            }
            resolution = *maybe_resolution;

            if (resolution.apply_to_all) {
                apply_to_all_resolution_ = resolution;
            }
        } else {
            // No dialog, skip conflicts
            continue;
        }

        // Check skip_identical with current resolution
        if (resolution.skip_identical && conflict.files_identical) {
            continue;
        }

        auto resolved = resolveConflict(conflict, resolution, resolution.move_replaced_to_trash);
        if (resolved) {
            resolved_items.push_back(std::move(*resolved));
        }
        // else: skip this file
    }

    return resolved_items;
}

std::optional<fs::ResolvedFileItem>
FileOperationManager::resolveConflict(const fs::FileConflictInfo& conflict,
                                      const fs::ConflictResolution& resolution,
                                      bool move_to_trash) {
    switch (resolution.action) {
    case fs::ConflictAction::Skip:
        return std::nullopt;

    case fs::ConflictAction::Overwrite:
        if (move_to_trash) {
            (void)fs::trashFile(conflict.dest_path, fs::TrashOptions{.silent = true});
        }
        return fs::ResolvedFileItem{
            .source_path = conflict.source_path,
            .dest_dir = conflict.dest_path.parent_path(),
        };

    case fs::ConflictAction::Rename:
        return fs::ResolvedFileItem{
            .source_path = conflict.source_path,
            .dest_dir = conflict.dest_path.parent_path(),
            .dest_name = resolution.custom_name,
        };

    case fs::ConflictAction::AutoNumber: {
        auto unique_path = fs::generateUniquePath(conflict.dest_path);
        return fs::ResolvedFileItem{
            .source_path = conflict.source_path,
            .dest_dir = unique_path.parent_path(),
            .dest_name = unique_path.filename().wstring(),
        };
    }

    case fs::ConflictAction::KeepNewer:
        if (conflict.source_time > conflict.dest_time) {
            if (move_to_trash) {
                (void)fs::trashFile(conflict.dest_path, fs::TrashOptions{.silent = true});
            }
            return fs::ResolvedFileItem{
                .source_path = conflict.source_path,
                .dest_dir = conflict.dest_path.parent_path(),
            };
        }
        return std::nullopt;  // Destination is newer, skip

    case fs::ConflictAction::KeepLarger:
        if (conflict.source_size > conflict.dest_size) {
            if (move_to_trash) {
                (void)fs::trashFile(conflict.dest_path, fs::TrashOptions{.silent = true});
            }
            return fs::ResolvedFileItem{
                .source_path = conflict.source_path,
                .dest_dir = conflict.dest_path.parent_path(),
            };
        }
        return std::nullopt;  // Destination is larger, skip

    default:
        return std::nullopt;
    }
}

// ============================================================================
// UI dialogs
// ============================================================================

std::optional<fs::ConflictResolution>
FileOperationManager::showConflictDialog(const fs::FileConflictInfo& conflict) {
    d2d::D2DFileConflictDialog dialog;
    return dialog.show(parent_window_, conflict);
}

void FileOperationManager::showErrorDialog(const fs::FileOperationResult& result,
                                           const std::wstring& operation) {
    std::wstring message;

    if (result.partiallySucceeded()) {
        auto processed = result.files_processed;
        auto failed = result.failed_files.size();
        message = std::vformat(i18n::tr("file_operation.partial_success"),
                               std::make_wformat_args(operation, processed, failed));
    } else {
        auto failed = result.failed_files.size();
        message = std::vformat(i18n::tr("file_operation.all_failed"),
                               std::make_wformat_args(operation, failed));
    }

    // Add details for first few failed files
    if (!result.failed_files.empty()) {
        message += std::wstring(i18n::tr("file_operation.failed_files_header"));
        size_t show_count = std::min(result.failed_files.size(), size_t(5));
        for (size_t i = 0; i < show_count; ++i) {
            message += std::wstring(i18n::tr("file_operation.failed_file_prefix"))
                + result.failed_files[i].filename().wstring();
        }
        if (result.failed_files.size() > show_count) {
            auto more = result.failed_files.size() - show_count;
            message += std::vformat(i18n::tr("file_operation.more_files"),
                                    std::make_wformat_args(more));
        }
    }

    MessageBoxW(parent_window_, message.c_str(),
                i18n::tr("file_operation.error_title").c_str(), MB_ICONWARNING | MB_OK);
}

}  // namespace nive::ui
