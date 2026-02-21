/// @file shell_file_operation.hpp
/// @brief Modern IFileOperation COM wrapper for batch file operations

#pragma once

#include <Windows.h>

#include <shobjidl_core.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../util/com_ptr.hpp"
#include "file_operations.hpp"

namespace nive::fs {

/// @brief A single file item with resolved destination for shell operations
struct ResolvedFileItem {
    std::filesystem::path source_path;
    std::filesystem::path dest_dir;
    std::optional<std::wstring> dest_name;  // Renamed filename, nullopt = keep original
};

/// @brief Options for shell file operations
struct ShellOperationOptions {
    HWND owner_window = nullptr;
    bool allow_undo = false;
    bool no_ui = false;  // Suppress all UI including progress dialog
};

/// @brief IFileOperationProgressSink implementation that tracks per-item results
class FileOperationSink : public IFileOperationProgressSink {
public:
    FileOperationSink() = default;
    virtual ~FileOperationSink() = default;

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IFileOperationProgressSink - pre-operation (no-ops)
    IFACEMETHODIMP StartOperations() override { return S_OK; }
    IFACEMETHODIMP FinishOperations(HRESULT) override { return S_OK; }
    IFACEMETHODIMP PreRenameItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return S_OK; }
    IFACEMETHODIMP PreDeleteItem(DWORD, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP PreNewItem(DWORD, IShellItem*, LPCWSTR) override { return S_OK; }

    // IFileOperationProgressSink - post-operation (collect results)
    IFACEMETHODIMP PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override;
    IFACEMETHODIMP PostMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT,
                                IShellItem*) override;
    IFACEMETHODIMP PostCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT,
                                IShellItem*) override;
    IFACEMETHODIMP PostDeleteItem(DWORD, IShellItem*, HRESULT, IShellItem*) override;
    IFACEMETHODIMP PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT,
                               IShellItem*) override {
        return S_OK;
    }

    // IFileOperationProgressSink - progress updates (no-ops)
    IFACEMETHODIMP UpdateProgress(UINT, UINT) override { return S_OK; }
    IFACEMETHODIMP ResetTimer() override { return S_OK; }
    IFACEMETHODIMP PauseTimer() override { return S_OK; }
    IFACEMETHODIMP ResumeTimer() override { return S_OK; }

    /// @brief Build FileOperationResult from collected data
    [[nodiscard]] FileOperationResult buildResult() const;

private:
    /// @brief Extract filesystem path from IShellItem
    static std::filesystem::path pathFromItem(IShellItem* item);

    LONG ref_count_ = 1;
    uint64_t success_count_ = 0;
    std::vector<std::filesystem::path> failed_files_;
};

/// @brief Modern IFileOperation wrapper for batch file operations
///
/// Provides OS-native progress dialog, undo support, and per-item
/// tracking via IFileOperationProgressSink.
class ShellFileOperation {
public:
    /// @brief Copy resolved items to their destinations
    [[nodiscard]] static FileOperationResult
    copyItems(std::span<const ResolvedFileItem> items, const ShellOperationOptions& options);

    /// @brief Move resolved items to their destinations
    [[nodiscard]] static FileOperationResult
    moveItems(std::span<const ResolvedFileItem> items, const ShellOperationOptions& options);

    /// @brief Delete items (permanent)
    [[nodiscard]] static FileOperationResult
    deleteItems(std::span<const std::filesystem::path> paths, const ShellOperationOptions& options);

    /// @brief Delete items to recycle bin
    [[nodiscard]] static FileOperationResult
    recycleItems(std::span<const std::filesystem::path> paths,
                 const ShellOperationOptions& options);

};

}  // namespace nive::fs
