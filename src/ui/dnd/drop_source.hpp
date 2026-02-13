/// @file drop_source.hpp
/// @brief OLE IDropSource implementation for drag operations

#pragma once

#include <Windows.h>

#include <ObjIdl.h>

namespace nive::ui {

/// @brief OLE IDropSource implementation
///
/// Provides feedback during drag operations and handles the Escape key
/// to cancel the drag.
class DropSource : public IDropSource {
public:
    DropSource();
    virtual ~DropSource() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDropSource
    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState) override;
    STDMETHOD(GiveFeedback)(DWORD dwEffect) override;

private:
    LONG ref_count_ = 1;
};

}  // namespace nive::ui
