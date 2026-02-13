/// @file file_operation_manager.cpp
/// @brief File operation manager implementation

#include "file_operation_manager.hpp"

#include <format>

#include "core/fs/trash.hpp"
#include "ui/d2d/dialog/delete_confirm/d2d_delete_confirm_dialog.hpp"
#include "ui/d2d/dialog/file_conflict/d2d_file_conflict_dialog.hpp"

namespace nive::ui {

FileOperationManager::FileOperationManager(HWND parent_window) : parent_window_(parent_window) {
}

fs::FileOperationResult
FileOperationManager::copyFiles(const std::vector<std::filesystem::path>& files,
                                const std::filesystem::path& dest_dir,
                                const FileOperationOptions& options) {
    apply_to_all_resolution_.reset();

    fs::ExtendedCopyOptions copy_options;
    copy_options.preserve_timestamps = true;

    if (options.show_conflict_dialog) {
        copy_options.on_conflict =
            [this](const fs::FileConflictInfo& conflict) -> std::optional<fs::ConflictResolution> {
            // Use "apply to all" if set
            if (apply_to_all_resolution_) {
                return apply_to_all_resolution_;
            }

            auto resolution = showConflictDialog(conflict);
            if (resolution && resolution->apply_to_all) {
                apply_to_all_resolution_ = resolution;
            }
            return resolution;
        };
    }

    auto result = fs::copyFilesWithConflictHandling(files, dest_dir, copy_options);

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, L"Copy");
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

    fs::ExtendedCopyOptions move_options;
    move_options.preserve_timestamps = true;

    if (options.show_conflict_dialog) {
        move_options.on_conflict =
            [this](const fs::FileConflictInfo& conflict) -> std::optional<fs::ConflictResolution> {
            if (apply_to_all_resolution_) {
                return apply_to_all_resolution_;
            }

            auto resolution = showConflictDialog(conflict);
            if (resolution && resolution->apply_to_all) {
                apply_to_all_resolution_ = resolution;
            }
            return resolution;
        };
    }

    auto result = fs::moveFilesWithConflictHandling(files, dest_dir, move_options);

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, L"Move");
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

        if (confirm_result == DeleteConfirmResult::Trash) {
            // Move to trash
            fs::TrashOptions trash_options;
            trash_options.silent = true;

            auto trash_result = fs::trashFiles(files, trash_options);

            result.files_processed = trash_result.files_trashed;
            result.failed_files.reserve(trash_result.failed_files.size());
            for (const auto& f : trash_result.failed_files) {
                result.failed_files.push_back(f);
            }

            if (trash_result.error != fs::TrashError::Success) {
                if (result.files_processed > 0) {
                    result.error = fs::FileOperationError::PartialSuccess;
                } else {
                    result.error = fs::FileOperationError::IoError;
                }
            }
        } else {
            // Permanent delete
            result = fs::deleteFiles(files);
        }
    } else {
        // Delete without confirmation (use trash by default)
        if (options.default_to_trash) {
            fs::TrashOptions trash_options;
            trash_options.silent = true;

            auto trash_result = fs::trashFiles(files, trash_options);

            result.files_processed = trash_result.files_trashed;
            result.failed_files.reserve(trash_result.failed_files.size());
            for (const auto& f : trash_result.failed_files) {
                result.failed_files.push_back(f);
            }

            if (trash_result.error != fs::TrashError::Success) {
                if (result.files_processed > 0) {
                    result.error = fs::FileOperationError::PartialSuccess;
                } else {
                    result.error = fs::FileOperationError::IoError;
                }
            }
        } else {
            result = fs::deleteFiles(files);
        }
    }

    if (!result.failed_files.empty() && result.error != fs::FileOperationError::Cancelled) {
        showErrorDialog(result, L"Delete");
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

std::optional<fs::ConflictResolution>
FileOperationManager::showConflictDialog(const fs::FileConflictInfo& conflict) {
    d2d::D2DFileConflictDialog dialog;
    return dialog.show(parent_window_, conflict);
}

void FileOperationManager::showErrorDialog(const fs::FileOperationResult& result,
                                           const std::wstring& operation) {
    std::wstring message;

    if (result.partiallySucceeded()) {
        message = std::format(
            L"{} operation completed with errors.\n\n"
            L"{} files processed successfully.\n"
            L"{} files failed.",
            operation, result.files_processed, result.failed_files.size());
    } else {
        message = std::format(
            L"{} operation failed.\n\n"
            L"{} files could not be processed.",
            operation, result.failed_files.size());
    }

    // Add details for first few failed files
    if (!result.failed_files.empty()) {
        message += L"\n\nFailed files:";
        size_t show_count = std::min(result.failed_files.size(), size_t(5));
        for (size_t i = 0; i < show_count; ++i) {
            message += L"\n- " + result.failed_files[i].filename().wstring();
        }
        if (result.failed_files.size() > show_count) {
            message += std::format(L"\n... and {} more", result.failed_files.size() - show_count);
        }
    }

    MessageBoxW(parent_window_, message.c_str(), L"File Operation Error", MB_ICONWARNING | MB_OK);
}

}  // namespace nive::ui
