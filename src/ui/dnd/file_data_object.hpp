/// @file file_data_object.hpp
/// @brief OLE IDataObject implementation for file drag operations

#pragma once

#include <Windows.h>

#include <ObjIdl.h>
#include <ShlObj.h>

#include <filesystem>
#include <vector>

namespace nive::ui {

/// @brief OLE IDataObject implementation for file paths
///
/// Provides file paths in CF_HDROP format for drag and drop operations.
class FileDataObject : public IDataObject {
public:
    /// @brief Create a data object with the given file paths
    explicit FileDataObject(const std::vector<std::filesystem::path>& paths);
    virtual ~FileDataObject();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDataObject
    STDMETHOD(GetData)(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override;
    STDMETHOD(GetDataHere)(FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    STDMETHOD(QueryGetData)(FORMATETC* pformatetc) override;
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) override;
    STDMETHOD(SetData)(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override;
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override;
    STDMETHOD(DAdvise)(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
                       DWORD* pdwConnection) override;
    STDMETHOD(DUnadvise)(DWORD dwConnection) override;
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA** ppenumAdvise) override;

private:
    HGLOBAL createHDrop() const;
    bool isFormatSupported(FORMATETC* pformatetc) const;

    LONG ref_count_ = 1;
    std::vector<std::filesystem::path> paths_;
};

/// @brief Create a FileDataObject with the given paths
IDataObject* createFileDataObject(const std::vector<std::filesystem::path>& paths);

}  // namespace nive::ui
