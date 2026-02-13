/// @file drop_source.cpp
/// @brief OLE IDropSource implementation

#include "drop_source.hpp"

namespace nive::ui {

DropSource::DropSource() = default;

STDMETHODIMP DropSource::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }

    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource)) {
        *ppv = static_cast<IDropSource*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DropSource::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) DropSource::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP DropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
    // Cancel drag if Escape is pressed
    if (fEscapePressed) {
        return DRAGDROP_S_CANCEL;
    }

    // Drop if left mouse button is released
    if (!(grfKeyState & MK_LBUTTON)) {
        return DRAGDROP_S_DROP;
    }

    // Continue the drag
    return S_OK;
}

STDMETHODIMP DropSource::GiveFeedback(DWORD dwEffect) {
    // Use default cursors
    return DRAGDROP_S_USEDEFAULTCURSORS;
}

}  // namespace nive::ui
