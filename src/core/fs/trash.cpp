/// @file trash.cpp
/// @brief Recycle bin operations implementation

#include "trash.hpp"
#include <Windows.h>

#include <ShlObj.h>

#include <sstream>

#include <shellapi.h>

namespace nive::fs {

namespace {

/// @brief Build null-terminated double-null terminated string for SHFileOperation
[[nodiscard]] std::wstring build_file_list(std::span<const std::filesystem::path> paths) {
    std::wstring result;

    for (const auto& path : paths) {
        result += path.wstring();
        result += L'\0';  // Null between items
    }
    result += L'\0';  // Double null at end

    return result;
}

/// @brief Convert SHFileOperation error to TrashError
[[nodiscard]] TrashError shfile_error_to_trash_error(int error) {
    switch (error) {
    case 0:
        return TrashError::Success;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case 0x402:  // DE_INVALIDFILES
    case 0x78:   // File not found (extended)
        return TrashError::NotFound;

    case ERROR_ACCESS_DENIED:
    case 0x75:  // DE_ACCESSDENIEDSRC
    case 0x76:  // DE_PATHTOODEEP
        return TrashError::AccessDenied;

    case ERROR_CANCELLED:
    case 0x7D:  // DE_OPCANCELLED
        return TrashError::Cancelled;

    default:
        return TrashError::IoError;
    }
}

}  // namespace

std::expected<void, TrashError> trashFile(const std::filesystem::path& path,
                                          const TrashOptions& options) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(TrashError::NotFound);
    }

    // Build double-null terminated path
    std::wstring file_list = path.wstring() + L'\0' + L'\0';

    SHFILEOPSTRUCTW file_op = {};
    file_op.hwnd = nullptr;
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = file_list.c_str();
    file_op.pTo = nullptr;
    file_op.fFlags = 0;

    if (options.allow_undo) {
        file_op.fFlags |= FOF_ALLOWUNDO;
    }
    if (!options.show_confirmation) {
        file_op.fFlags |= FOF_NOCONFIRMATION;
    }
    if (!options.show_progress || options.silent) {
        file_op.fFlags |= FOF_SILENT;
    }
    if (options.silent) {
        file_op.fFlags |= FOF_NOERRORUI;
    }

    int result = SHFileOperationW(&file_op);

    if (result != 0) {
        return std::unexpected(shfile_error_to_trash_error(result));
    }

    if (file_op.fAnyOperationsAborted) {
        return std::unexpected(TrashError::Cancelled);
    }

    return {};
}

TrashResult trashFiles(std::span<const std::filesystem::path> paths, const TrashOptions& options) {
    TrashResult result;

    if (paths.empty()) {
        return result;
    }

    // Build file list
    std::wstring file_list = build_file_list(paths);

    SHFILEOPSTRUCTW file_op = {};
    file_op.hwnd = nullptr;
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = file_list.c_str();
    file_op.pTo = nullptr;
    file_op.fFlags = 0;

    if (options.allow_undo) {
        file_op.fFlags |= FOF_ALLOWUNDO;
    }
    if (!options.show_confirmation) {
        file_op.fFlags |= FOF_NOCONFIRMATION;
    }
    if (!options.show_progress || options.silent) {
        file_op.fFlags |= FOF_SILENT;
    }
    if (options.silent) {
        file_op.fFlags |= FOF_NOERRORUI;
    }

    // Allow partial success
    file_op.fFlags |= FOF_NOCONFIRMMKDIR;

    int op_result = SHFileOperationW(&file_op);

    if (op_result != 0) {
        result.error = shfile_error_to_trash_error(op_result);
        // All files failed
        for (const auto& path : paths) {
            result.failed_files.push_back(path);
        }
        return result;
    }

    if (file_op.fAnyOperationsAborted) {
        result.error = TrashError::Cancelled;
        return result;
    }

    // Success - all files trashed
    result.files_trashed = paths.size();
    return result;
}

std::expected<void, TrashError> permanentDelete(const std::filesystem::path& path,
                                                const TrashOptions& options) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(TrashError::NotFound);
    }

    std::wstring file_list = path.wstring() + L'\0' + L'\0';

    SHFILEOPSTRUCTW file_op = {};
    file_op.hwnd = nullptr;
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = file_list.c_str();
    file_op.pTo = nullptr;
    file_op.fFlags = 0;

    // No FOF_ALLOWUNDO = permanent delete
    if (!options.show_confirmation) {
        file_op.fFlags |= FOF_NOCONFIRMATION;
    }
    if (!options.show_progress || options.silent) {
        file_op.fFlags |= FOF_SILENT;
    }
    if (options.silent) {
        file_op.fFlags |= FOF_NOERRORUI;
    }

    int result = SHFileOperationW(&file_op);

    if (result != 0) {
        return std::unexpected(shfile_error_to_trash_error(result));
    }

    if (file_op.fAnyOperationsAborted) {
        return std::unexpected(TrashError::Cancelled);
    }

    return {};
}

TrashResult permanentDeleteFiles(std::span<const std::filesystem::path> paths,
                                 const TrashOptions& options) {
    TrashResult result;

    if (paths.empty()) {
        return result;
    }

    std::wstring file_list = build_file_list(paths);

    SHFILEOPSTRUCTW file_op = {};
    file_op.hwnd = nullptr;
    file_op.wFunc = FO_DELETE;
    file_op.pFrom = file_list.c_str();
    file_op.pTo = nullptr;
    file_op.fFlags = 0;

    // No FOF_ALLOWUNDO = permanent delete
    if (!options.show_confirmation) {
        file_op.fFlags |= FOF_NOCONFIRMATION;
    }
    if (!options.show_progress || options.silent) {
        file_op.fFlags |= FOF_SILENT;
    }
    if (options.silent) {
        file_op.fFlags |= FOF_NOERRORUI;
    }

    int op_result = SHFileOperationW(&file_op);

    if (op_result != 0) {
        result.error = shfile_error_to_trash_error(op_result);
        for (const auto& path : paths) {
            result.failed_files.push_back(path);
        }
        return result;
    }

    if (file_op.fAnyOperationsAborted) {
        result.error = TrashError::Cancelled;
        return result;
    }

    result.files_trashed = paths.size();
    return result;
}

std::expected<void, TrashError> emptyRecycleBin(const std::wstring& drive_letter,
                                                bool show_confirmation, bool show_progress) {
    const wchar_t* root_path = drive_letter.empty() ? nullptr : drive_letter.c_str();

    DWORD flags = 0;
    if (!show_confirmation) {
        flags |= SHERB_NOCONFIRMATION;
    }
    if (!show_progress) {
        flags |= SHERB_NOPROGRESSUI;
    }
    flags |= SHERB_NOSOUND;

    HRESULT hr = SHEmptyRecycleBinW(nullptr, root_path, flags);

    // S_OK or S_FALSE (already empty) are both success
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        if (hr == E_ACCESSDENIED) {
            return std::unexpected(TrashError::AccessDenied);
        }
        return std::unexpected(TrashError::IoError);
    }

    return {};
}

std::expected<std::pair<uint64_t, uint64_t>, TrashError>
getRecycleBinInfo(const std::wstring& drive_letter) {
    const wchar_t* root_path = drive_letter.empty() ? nullptr : drive_letter.c_str();

    SHQUERYRBINFO info = {};
    info.cbSize = sizeof(info);

    HRESULT hr = SHQueryRecycleBinW(root_path, &info);

    if (FAILED(hr)) {
        return std::unexpected(TrashError::IoError);
    }

    return std::make_pair(info.i64NumItems, info.i64Size);
}

}  // namespace nive::fs
