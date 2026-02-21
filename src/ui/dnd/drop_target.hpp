/// @file drop_target.hpp
/// @brief OLE IDropTarget implementation for receiving file drops

#pragma once

#include <Windows.h>

#include <ObjIdl.h>
#include <ShlObj.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace nive::ui {

/// @brief Callback for file drop operations
/// @param files List of dropped file paths
/// @param dest_path Destination directory path
/// @param effect Drop effect (DROPEFFECT_COPY or DROPEFFECT_MOVE)
using FileDropCallback = std::function<void(const std::vector<std::filesystem::path>& files,
                                            const std::filesystem::path& dest_path, DWORD effect)>;

/// @brief Callback to get destination path from drop point
/// @param pt Drop point in screen coordinates
/// @return Destination directory path, or empty if invalid drop location
using GetDropPathCallback = std::function<std::filesystem::path(POINT pt)>;

/// @brief OLE IDropTarget implementation
///
/// Handles file drops from other applications or within the same application.
class DropTarget : public IDropTarget {
public:
    /// @brief Create a drop target
    /// @param hwnd Window to receive drops
    /// @param on_drop Callback when files are dropped
    /// @param get_drop_path Callback to get destination path from drop point
    DropTarget(HWND hwnd, FileDropCallback on_drop, GetDropPathCallback get_drop_path);
    virtual ~DropTarget();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDropTarget
    STDMETHOD(DragEnter)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
                         DWORD* pdwEffect) override;
    STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHOD(DragLeave)() override;
    STDMETHOD(Drop)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

    /// @brief Set callback invoked when drag operation ends (DragLeave or Drop)
    void onDragEnd(std::function<void()> callback) { on_drag_end_ = std::move(callback); }

    /// @brief Register this drop target with the window
    bool registerTarget();

    /// @brief Revoke the drop target registration
    void revokeTarget();

    /// @brief Calculate drop effect based on key state and source/dest drives
    ///
    /// Explorer-compatible behavior:
    /// - Same drive: Default = Move, Ctrl = Copy
    /// - Different drive: Default = Copy, Shift = Move
    ///
    /// @param grfKeyState Key state flags
    /// @param source_path First source file path
    /// @param dest_path Destination directory path
    /// @return Drop effect (DROPEFFECT_COPY or DROPEFFECT_MOVE)
    static DWORD calculateDropEffect(DWORD grfKeyState, const std::filesystem::path& source_path,
                                     const std::filesystem::path& dest_path);

private:
    bool hasFileDrop(IDataObject* pDataObj);
    std::vector<std::filesystem::path> extractFilePaths(IDataObject* pDataObj);

    LONG ref_count_ = 1;
    HWND hwnd_;
    FileDropCallback on_drop_;
    GetDropPathCallback get_drop_path_;
    std::function<void()> on_drag_end_;

    // State during drag
    bool has_valid_data_ = false;
    std::filesystem::path current_dest_path_;
    std::filesystem::path first_source_path_;
    bool registered_ = false;
};

}  // namespace nive::ui
