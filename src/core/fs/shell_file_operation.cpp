/// @file shell_file_operation.cpp
/// @brief IFileOperation COM wrapper implementation

#include "shell_file_operation.hpp"

#include <Windows.h>

#include <ShlObj.h>
#include <shellapi.h>
#include <shobjidl_core.h>

namespace nive::fs {

// ============================================================================
// FileOperationSink - IUnknown
// ============================================================================

IFACEMETHODIMP FileOperationSink::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IFileOperationProgressSink) {
        *ppv = static_cast<IFileOperationProgressSink*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) FileOperationSink::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) FileOperationSink::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

// ============================================================================
// FileOperationSink - Post-operation callbacks
// ============================================================================

IFACEMETHODIMP FileOperationSink::PostRenameItem(DWORD, IShellItem* item, LPCWSTR,
                                                  HRESULT hr_result, IShellItem*) {
    if (SUCCEEDED(hr_result)) {
        ++success_count_;
    } else {
        failed_files_.push_back(pathFromItem(item));
    }
    return S_OK;
}

IFACEMETHODIMP FileOperationSink::PostMoveItem(DWORD, IShellItem* item, IShellItem*, LPCWSTR,
                                                HRESULT hr_result, IShellItem*) {
    if (SUCCEEDED(hr_result)) {
        ++success_count_;
    } else {
        failed_files_.push_back(pathFromItem(item));
    }
    return S_OK;
}

IFACEMETHODIMP FileOperationSink::PostCopyItem(DWORD, IShellItem* item, IShellItem*, LPCWSTR,
                                                HRESULT hr_result, IShellItem*) {
    if (SUCCEEDED(hr_result)) {
        ++success_count_;
    } else {
        failed_files_.push_back(pathFromItem(item));
    }
    return S_OK;
}

IFACEMETHODIMP FileOperationSink::PostDeleteItem(DWORD, IShellItem* item, HRESULT hr_result,
                                                  IShellItem*) {
    if (SUCCEEDED(hr_result)) {
        ++success_count_;
    } else {
        failed_files_.push_back(pathFromItem(item));
    }
    return S_OK;
}

// ============================================================================
// FileOperationSink - Result building
// ============================================================================

FileOperationResult FileOperationSink::buildResult() const {
    FileOperationResult result;
    result.files_processed = success_count_;
    result.failed_files = failed_files_;

    if (!failed_files_.empty()) {
        result.error = success_count_ > 0 ? FileOperationError::PartialSuccess
                                          : FileOperationError::IoError;
    }
    return result;
}

std::filesystem::path FileOperationSink::pathFromItem(IShellItem* item) {
    if (!item) {
        return {};
    }
    LPWSTR name = nullptr;
    HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &name);
    if (SUCCEEDED(hr) && name) {
        std::filesystem::path path(name);
        CoTaskMemFree(name);
        return path;
    }
    return {};
}

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/// @brief Create and configure IFileOperation instance
[[nodiscard]] HRESULT initOperation(ComPtr<IFileOperation>& op,
                                    const ShellOperationOptions& options, bool recycle) {
    HRESULT hr =
        CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(op.GetAddressOf()));
    if (FAILED(hr)) {
        return hr;
    }

    // Base flags: suppress built-in conflict/error UI (we handle conflicts pre-operation)
    DWORD flags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI;

    if (options.no_ui) {
        flags |= FOF_SILENT;
    }
    if (recycle) {
        flags |= FOFX_RECYCLEONDELETE;
    }
    if (options.allow_undo) {
        flags |= FOFX_ADDUNDORECORD;
    }

    hr = op->SetOperationFlags(flags);
    if (FAILED(hr)) {
        return hr;
    }

    if (options.owner_window) {
        hr = op->SetOwnerWindow(options.owner_window);
        if (FAILED(hr)) {
            return hr;
        }
    }

    return S_OK;
}

/// @brief Create IShellItem from filesystem path
[[nodiscard]] HRESULT createShellItem(const std::filesystem::path& path,
                                      ComPtr<IShellItem>& item) {
    return SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(item.GetAddressOf()));
}

/// @brief Convert HRESULT to FileOperationError
[[nodiscard]] FileOperationError hresultToError(HRESULT hr) {
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) || hr == E_ABORT) {
        return FileOperationError::Cancelled;
    }
    if (hr == E_ACCESSDENIED) {
        return FileOperationError::AccessDenied;
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
        return FileOperationError::NotFound;
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL)) {
        return FileOperationError::DiskFull;
    }
    return FileOperationError::IoError;
}

/// @brief Execute IFileOperation and collect results from sink
[[nodiscard]] FileOperationResult executeAndCollect(ComPtr<IFileOperation>& op,
                                                    FileOperationSink* sink) {
    HRESULT hr = op->PerformOperations();

    FileOperationResult result = sink->buildResult();

    // If PerformOperations itself failed and sink has no data, report the error
    if (FAILED(hr) && result.files_processed == 0 && result.failed_files.empty()) {
        result.error = hresultToError(hr);
    }

    // Check if user cancelled via progress dialog
    BOOL any_aborted = FALSE;
    if (SUCCEEDED(op->GetAnyOperationsAborted(&any_aborted)) && any_aborted) {
        result.error = FileOperationError::Cancelled;
    }

    return result;
}

/// @brief Shared implementation for delete/recycle operations
[[nodiscard]] FileOperationResult deleteItemsImpl(std::span<const std::filesystem::path> paths,
                                                   const ShellOperationOptions& options,
                                                   bool recycle) {
    if (paths.empty()) {
        return {};
    }

    ComPtr<IFileOperation> op;
    HRESULT hr = initOperation(op, options, recycle);
    if (FAILED(hr)) {
        FileOperationResult result;
        result.error = hresultToError(hr);
        return result;
    }

    auto* sink = new FileOperationSink();
    DWORD cookie = 0;
    op->Advise(sink, &cookie);

    for (const auto& path : paths) {
        ComPtr<IShellItem> item;
        hr = createShellItem(path, item);
        if (FAILED(hr)) {
            continue;
        }
        op->DeleteItem(item.Get(), nullptr);
    }

    auto result = executeAndCollect(op, sink);

    op->Unadvise(cookie);
    sink->Release();

    return result;
}

}  // namespace

// ============================================================================
// ShellFileOperation - Public API
// ============================================================================

FileOperationResult ShellFileOperation::copyItems(std::span<const ResolvedFileItem> items,
                                                   const ShellOperationOptions& options) {
    if (items.empty()) {
        return {};
    }

    ComPtr<IFileOperation> op;
    HRESULT hr = initOperation(op, options, false);
    if (FAILED(hr)) {
        FileOperationResult result;
        result.error = hresultToError(hr);
        return result;
    }

    auto* sink = new FileOperationSink();
    DWORD cookie = 0;
    op->Advise(sink, &cookie);

    for (const auto& item : items) {
        ComPtr<IShellItem> source;
        hr = createShellItem(item.source_path, source);
        if (FAILED(hr)) {
            continue;
        }

        ComPtr<IShellItem> dest_folder;
        hr = createShellItem(item.dest_dir, dest_folder);
        if (FAILED(hr)) {
            continue;
        }

        LPCWSTR new_name = item.dest_name ? item.dest_name->c_str() : nullptr;
        op->CopyItem(source.Get(), dest_folder.Get(), new_name, nullptr);
    }

    auto result = executeAndCollect(op, sink);

    op->Unadvise(cookie);
    sink->Release();

    return result;
}

FileOperationResult ShellFileOperation::moveItems(std::span<const ResolvedFileItem> items,
                                                   const ShellOperationOptions& options) {
    if (items.empty()) {
        return {};
    }

    ComPtr<IFileOperation> op;
    HRESULT hr = initOperation(op, options, false);
    if (FAILED(hr)) {
        FileOperationResult result;
        result.error = hresultToError(hr);
        return result;
    }

    auto* sink = new FileOperationSink();
    DWORD cookie = 0;
    op->Advise(sink, &cookie);

    for (const auto& item : items) {
        ComPtr<IShellItem> source;
        hr = createShellItem(item.source_path, source);
        if (FAILED(hr)) {
            continue;
        }

        ComPtr<IShellItem> dest_folder;
        hr = createShellItem(item.dest_dir, dest_folder);
        if (FAILED(hr)) {
            continue;
        }

        LPCWSTR new_name = item.dest_name ? item.dest_name->c_str() : nullptr;
        op->MoveItem(source.Get(), dest_folder.Get(), new_name, nullptr);
    }

    auto result = executeAndCollect(op, sink);

    op->Unadvise(cookie);
    sink->Release();

    return result;
}

FileOperationResult ShellFileOperation::deleteItems(std::span<const std::filesystem::path> paths,
                                                     const ShellOperationOptions& options) {
    return deleteItemsImpl(paths, options, false);
}

FileOperationResult ShellFileOperation::recycleItems(std::span<const std::filesystem::path> paths,
                                                      const ShellOperationOptions& options) {
    return deleteItemsImpl(paths, options, true);
}

}  // namespace nive::fs
