/// @file file_data_object.cpp
/// @brief OLE IDataObject implementation for files

#include "file_data_object.hpp"

namespace nive::ui {

FileDataObject::FileDataObject(const std::vector<std::filesystem::path>& paths) : paths_(paths) {
}

FileDataObject::~FileDataObject() = default;

STDMETHODIMP FileDataObject::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }

    *ppv = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject)) {
        *ppv = static_cast<IDataObject*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) FileDataObject::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) FileDataObject::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP FileDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) {
    if (!pformatetcIn || !pmedium) {
        return E_INVALIDARG;
    }

    if (!isFormatSupported(pformatetcIn)) {
        return DV_E_FORMATETC;
    }

    // Create HDROP data
    HGLOBAL hGlobal = createHDrop();
    if (!hGlobal) {
        return E_OUTOFMEMORY;
    }

    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->hGlobal = hGlobal;
    pmedium->pUnkForRelease = nullptr;

    return S_OK;
}

STDMETHODIMP FileDataObject::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) {
    return E_NOTIMPL;
}

STDMETHODIMP FileDataObject::QueryGetData(FORMATETC* pformatetc) {
    if (!pformatetc) {
        return E_INVALIDARG;
    }

    return isFormatSupported(pformatetc) ? S_OK : DV_E_FORMATETC;
}

STDMETHODIMP FileDataObject::GetCanonicalFormatEtc(FORMATETC* pformatectIn,
                                                   FORMATETC* pformatetcOut) {
    if (!pformatetcOut) {
        return E_INVALIDARG;
    }

    pformatetcOut->ptd = nullptr;
    return DATA_S_SAMEFORMATETC;
}

STDMETHODIMP FileDataObject::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) {
    return E_NOTIMPL;
}

STDMETHODIMP FileDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) {
    if (!ppenumFormatEtc) {
        return E_INVALIDARG;
    }

    *ppenumFormatEtc = nullptr;

    if (dwDirection != DATADIR_GET) {
        return E_NOTIMPL;
    }

    // Create a static array of supported formats
    static FORMATETC formats[] = {
        {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}
    };

    return SHCreateStdEnumFmtEtc(1, formats, ppenumFormatEtc);
}

STDMETHODIMP FileDataObject::DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
                                     DWORD* pdwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP FileDataObject::DUnadvise(DWORD dwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP FileDataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise) {
    return OLE_E_ADVISENOTSUPPORTED;
}

HGLOBAL FileDataObject::createHDrop() const {
    if (paths_.empty()) {
        return nullptr;
    }

    // Calculate total size needed
    // DROPFILES structure + null-terminated file paths + final null terminator
    size_t total_size = sizeof(DROPFILES);
    for (const auto& path : paths_) {
        total_size += (path.wstring().length() + 1) * sizeof(wchar_t);
    }
    total_size += sizeof(wchar_t);  // Final null terminator

    // Allocate memory
    HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, total_size);
    if (!hGlobal) {
        return nullptr;
    }

    auto* pDropFiles = static_cast<DROPFILES*>(GlobalLock(hGlobal));
    if (!pDropFiles) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    // Fill DROPFILES structure
    pDropFiles->pFiles = sizeof(DROPFILES);
    pDropFiles->pt.x = 0;
    pDropFiles->pt.y = 0;
    pDropFiles->fNC = FALSE;
    pDropFiles->fWide = TRUE;  // Unicode paths

    // Copy file paths
    auto* dest =
        reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(pDropFiles) + sizeof(DROPFILES));
    for (const auto& path : paths_) {
        const auto& str = path.wstring();
        wcscpy(dest, str.c_str());
        dest += str.length() + 1;
    }
    *dest = L'\0';  // Final null terminator

    GlobalUnlock(hGlobal);
    return hGlobal;
}

bool FileDataObject::isFormatSupported(FORMATETC* pformatetc) const {
    if (pformatetc->cfFormat != CF_HDROP) {
        return false;
    }

    if (pformatetc->dwAspect != DVASPECT_CONTENT) {
        return false;
    }

    if (!(pformatetc->tymed & TYMED_HGLOBAL)) {
        return false;
    }

    return true;
}

IDataObject* createFileDataObject(const std::vector<std::filesystem::path>& paths) {
    return new FileDataObject(paths);
}

}  // namespace nive::ui
