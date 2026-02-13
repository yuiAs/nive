/// @file drop_target.cpp
/// @brief OLE IDropTarget implementation

#include "drop_target.hpp"

#include <cwctype>

#include <shellapi.h>

namespace nive::ui {

DropTarget::DropTarget(HWND hwnd, FileDropCallback on_drop, GetDropPathCallback get_drop_path)
    : hwnd_(hwnd), on_drop_(std::move(on_drop)), get_drop_path_(std::move(get_drop_path)) {
}

DropTarget::~DropTarget() {
    revokeTarget();
}

STDMETHODIMP DropTarget::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }

    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropTarget)) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DropTarget::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) DropTarget::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
                                   DWORD* pdwEffect) {
    if (!pdwEffect) {
        return E_INVALIDARG;
    }

    has_valid_data_ = hasFileDrop(pDataObj);
    first_source_path_.clear();

    if (!has_valid_data_) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Get first source path for drive comparison
    auto paths = extractFilePaths(pDataObj);
    if (!paths.empty()) {
        first_source_path_ = paths[0];
    }

    // Get destination path
    POINT screenPt = {pt.x, pt.y};
    current_dest_path_ = get_drop_path_ ? get_drop_path_(screenPt) : std::filesystem::path{};

    if (current_dest_path_.empty()) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Calculate effect
    *pdwEffect = calculateDropEffect(grfKeyState, first_source_path_, current_dest_path_);

    return S_OK;
}

STDMETHODIMP DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (!pdwEffect) {
        return E_INVALIDARG;
    }

    if (!has_valid_data_) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Update destination path
    POINT screenPt = {pt.x, pt.y};
    current_dest_path_ = get_drop_path_ ? get_drop_path_(screenPt) : std::filesystem::path{};

    if (current_dest_path_.empty()) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Calculate effect
    *pdwEffect = calculateDropEffect(grfKeyState, first_source_path_, current_dest_path_);

    return S_OK;
}

STDMETHODIMP DropTarget::DragLeave() {
    has_valid_data_ = false;
    current_dest_path_.clear();
    first_source_path_.clear();
    return S_OK;
}

STDMETHODIMP DropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
                              DWORD* pdwEffect) {
    if (!pdwEffect) {
        return E_INVALIDARG;
    }

    if (!has_valid_data_) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Get destination path
    POINT screenPt = {pt.x, pt.y};
    auto dest_path = get_drop_path_ ? get_drop_path_(screenPt) : std::filesystem::path{};

    if (dest_path.empty()) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Extract file paths
    auto paths = extractFilePaths(pDataObj);
    if (paths.empty()) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // Calculate final effect
    DWORD effect = calculateDropEffect(grfKeyState, paths[0], dest_path);
    *pdwEffect = effect;

    // Call the callback
    if (on_drop_) {
        on_drop_(paths, dest_path, effect);
    }

    // Clean up
    has_valid_data_ = false;
    current_dest_path_.clear();
    first_source_path_.clear();

    return S_OK;
}

bool DropTarget::registerTarget() {
    if (registered_) {
        return true;
    }

    HRESULT hr = RegisterDragDrop(hwnd_, this);
    if (SUCCEEDED(hr)) {
        registered_ = true;
        return true;
    }
    return false;
}

void DropTarget::revokeTarget() {
    if (registered_) {
        RevokeDragDrop(hwnd_);
        registered_ = false;
    }
}

DWORD DropTarget::calculateDropEffect(DWORD grfKeyState, const std::filesystem::path& source_path,
                                      const std::filesystem::path& dest_path) {
    if (source_path.empty() || dest_path.empty()) {
        return DROPEFFECT_COPY;  // Default to copy if paths unknown
    }

    auto source_str = source_path.wstring();
    auto dest_str = dest_path.wstring();

    // Check if on same drive (compare first character, case-insensitive)
    bool same_drive = false;
    if (source_str.length() >= 1 && dest_str.length() >= 1) {
        same_drive = (std::towupper(source_str[0]) == std::towupper(dest_str[0]));
    }

    bool ctrl = (grfKeyState & MK_CONTROL) != 0;
    bool shift = (grfKeyState & MK_SHIFT) != 0;

    if (same_drive) {
        // Same drive: Default = Move, Ctrl = Copy
        return ctrl ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
    } else {
        // Different drive: Default = Copy, Shift = Move
        return shift ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    }
}

bool DropTarget::hasFileDrop(IDataObject* pDataObj) {
    if (!pDataObj) {
        return false;
    }

    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return SUCCEEDED(pDataObj->QueryGetData(&fmt));
}

std::vector<std::filesystem::path> DropTarget::extractFilePaths(IDataObject* pDataObj) {
    std::vector<std::filesystem::path> paths;

    if (!pDataObj) {
        return paths;
    }

    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg = {};

    if (FAILED(pDataObj->GetData(&fmt, &stg))) {
        return paths;
    }

    auto* pDropFiles = static_cast<DROPFILES*>(GlobalLock(stg.hGlobal));
    if (!pDropFiles) {
        ReleaseStgMedium(&stg);
        return paths;
    }

    // Get file count
    UINT count = DragQueryFileW(reinterpret_cast<HDROP>(pDropFiles), 0xFFFFFFFF, nullptr, 0);

    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(reinterpret_cast<HDROP>(pDropFiles), i, nullptr, 0);
        if (len > 0) {
            std::wstring path(len + 1, L'\0');
            DragQueryFileW(reinterpret_cast<HDROP>(pDropFiles), i, path.data(), len + 1);
            path.resize(len);
            paths.emplace_back(path);
        }
    }

    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);

    return paths;
}

}  // namespace nive::ui
