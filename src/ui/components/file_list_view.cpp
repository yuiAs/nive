/// @file file_list_view.cpp
/// @brief File list view implementation

#include "file_list_view.hpp"

#include <chrono>
#include <format>

namespace nive::ui {

FileListView::FileListView() = default;
FileListView::~FileListView() = default;

bool FileListView::create(HWND parent, HINSTANCE hInstance, int id) {
    hwnd_ = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
                            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT |
                                LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
                            0, 0, 100, 100, parent,
                            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)), hInstance, nullptr);

    if (!hwnd_) {
        return false;
    }

    // Enable extended styles
    ListView_SetExtendedListViewStyle(hwnd_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                                 LVS_EX_HEADERDRAGDROP);

    // Get header control
    header_ = ListView_GetHeader(hwnd_);

    // Create columns
    createColumns();

    return true;
}

void FileListView::setBounds(int x, int y, int width, int height) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

void FileListView::setItems(const std::vector<fs::FileMetadata>& items) {
    items_ = items;
    populateItems();
}

std::vector<size_t> FileListView::selectedIndices() const {
    std::vector<size_t> result;
    int index = -1;
    while ((index = ListView_GetNextItem(hwnd_, index, LVNI_SELECTED)) != -1) {
        result.push_back(static_cast<size_t>(index));
    }
    return result;
}

void FileListView::setSelection(const std::vector<size_t>& indices) {
    // Clear all selection first
    ListView_SetItemState(hwnd_, -1, 0, LVIS_SELECTED);

    // Set new selection
    for (size_t idx : indices) {
        if (idx < items_.size()) {
            ListView_SetItemState(hwnd_, static_cast<int>(idx), LVIS_SELECTED, LVIS_SELECTED);
        }
    }
}

void FileListView::selectSingle(size_t index) {
    if (index < items_.size()) {
        ListView_SetItemState(hwnd_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(hwnd_, static_cast<int>(index), LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void FileListView::clearSelection() {
    ListView_SetItemState(hwnd_, -1, 0, LVIS_SELECTED);
}

void FileListView::ensureVisible(size_t index) {
    if (index < items_.size()) {
        ListView_EnsureVisible(hwnd_, static_cast<int>(index), FALSE);
    }
}

void FileListView::setSort(FileListColumn column, bool ascending) {
    sort_column_ = column;
    sort_ascending_ = ascending;
    // TODO: Update header arrow indicator
}

std::array<int, 5> FileListView::getColumnWidths() const {
    std::array<int, 5> widths = {0, 0, 0, 0, 0};
    if (hwnd_) {
        for (int i = 0; i < 5; ++i) {
            widths[i] = ListView_GetColumnWidth(hwnd_, i);
        }
    }
    return widths;
}

void FileListView::setColumnWidths(const std::array<int, 5>& widths) {
    if (hwnd_) {
        for (int i = 0; i < 5; ++i) {
            if (widths[i] > 0) {
                ListView_SetColumnWidth(hwnd_, i, widths[i]);
            }
        }
    }
}

std::vector<std::filesystem::path> FileListView::selectedFilePaths() const {
    std::vector<std::filesystem::path> paths;
    auto indices = selectedIndices();
    for (auto i : indices) {
        if (i < items_.size() && !items_[i].is_in_archive()) {
            paths.push_back(items_[i].path);
        }
    }
    return paths;
}

void FileListView::setResolution(const std::filesystem::path& path, uint32_t width,
                                  uint32_t height) {
    if (!hwnd_ || width == 0 || height == 0) {
        return;
    }

    std::wstring key = path.wstring();
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].sourceIdentifier() == key) {
            auto text = formatResolution(width, height);
            ListView_SetItemText(hwnd_, static_cast<int>(i), 4,
                                 const_cast<LPWSTR>(text.c_str()));
            return;
        }
    }
}

void FileListView::refresh() {
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool FileListView::handleNotify(NMHDR* nmhdr) {
    switch (nmhdr->code) {
    case NM_DBLCLK: {
        auto* nmia = reinterpret_cast<NMITEMACTIVATE*>(nmhdr);
        if (nmia->iItem >= 0 && item_activated_callback_) {
            item_activated_callback_(static_cast<size_t>(nmia->iItem));
        }
        return true;
    }

    case LVN_ITEMCHANGED: {
        auto* nmlv = reinterpret_cast<NMLISTVIEW*>(nmhdr);
        if ((nmlv->uChanged & LVIF_STATE) &&
            ((nmlv->uNewState ^ nmlv->uOldState) & LVIS_SELECTED)) {
            if (selection_changed_callback_) {
                selection_changed_callback_(selectedIndices());
            }
        }
        return true;
    }

    case LVN_COLUMNCLICK: {
        auto* nmlv = reinterpret_cast<NMLISTVIEW*>(nmhdr);
        auto column = static_cast<FileListColumn>(nmlv->iSubItem);
        bool ascending = (column == sort_column_) ? !sort_ascending_ : true;
        sort_column_ = column;
        sort_ascending_ = ascending;
        if (sort_changed_callback_) {
            sort_changed_callback_(column, ascending);
        }
        return true;
    }

    case NM_RETURN: {
        auto indices = selectedIndices();
        if (!indices.empty() && item_activated_callback_) {
            item_activated_callback_(indices[0]);
        }
        return true;
    }

    case LVN_KEYDOWN: {
        auto* nmlvk = reinterpret_cast<NMLVKEYDOWN*>(nmhdr);
        if (nmlvk->wVKey == VK_DELETE) {
            auto paths = selectedFilePaths();
            if (!paths.empty() && delete_requested_callback_) {
                delete_requested_callback_(paths);
            }
        }
        return true;
    }
    }

    return false;
}

void FileListView::createColumns() {
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    // Name column
    lvc.pszText = const_cast<LPWSTR>(L"Name");
    lvc.cx = 250;
    lvc.iSubItem = 0;
    ListView_InsertColumn(hwnd_, 0, &lvc);

    // Size column
    lvc.pszText = const_cast<LPWSTR>(L"Size");
    lvc.cx = 80;
    lvc.iSubItem = 1;
    ListView_InsertColumn(hwnd_, 1, &lvc);

    // Date column
    lvc.pszText = const_cast<LPWSTR>(L"Date");
    lvc.cx = 130;
    lvc.iSubItem = 2;
    ListView_InsertColumn(hwnd_, 2, &lvc);

    // Path column (archive internal directory)
    lvc.pszText = const_cast<LPWSTR>(L"Path");
    lvc.cx = 150;
    lvc.iSubItem = 3;
    ListView_InsertColumn(hwnd_, 3, &lvc);

    // Resolution column
    lvc.pszText = const_cast<LPWSTR>(L"Resolution");
    lvc.cx = 100;
    lvc.iSubItem = 4;
    ListView_InsertColumn(hwnd_, 4, &lvc);
}

void FileListView::populateItems() {
    // Disable redraw during population
    SendMessageW(hwnd_, WM_SETREDRAW, FALSE, 0);

    ListView_DeleteAllItems(hwnd_);

    LVITEMW lvi = {};
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];

        // Name
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(item.name.c_str());
        ListView_InsertItem(hwnd_, &lvi);

        // Size
        auto size_str = item.is_directory() ? L"-" : formatSize(item.size_bytes);
        ListView_SetItemText(hwnd_, static_cast<int>(i), 1, const_cast<LPWSTR>(size_str.c_str()));

        // Date
        auto date_str = formatDate(item.modified_time);
        ListView_SetItemText(hwnd_, static_cast<int>(i), 2, const_cast<LPWSTR>(date_str.c_str()));

        // Path (archive internal directory)
        auto path_str = formatArchivePath(item);
        ListView_SetItemText(hwnd_, static_cast<int>(i), 3, const_cast<LPWSTR>(path_str.c_str()));

        // Resolution (not available in FileMetadata, show "-" for now)
        ListView_SetItemText(hwnd_, static_cast<int>(i), 4, const_cast<LPWSTR>(L"-"));
    }

    // Re-enable redraw
    SendMessageW(hwnd_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void FileListView::updateItem(size_t index) {
    if (index >= items_.size()) {
        return;
    }

    const auto& item = items_[index];
    int i = static_cast<int>(index);

    ListView_SetItemText(hwnd_, i, 0, const_cast<LPWSTR>(item.name.c_str()));

    auto size_str = item.is_directory() ? L"-" : formatSize(item.size_bytes);
    ListView_SetItemText(hwnd_, i, 1, const_cast<LPWSTR>(size_str.c_str()));

    auto date_str = formatDate(item.modified_time);
    ListView_SetItemText(hwnd_, i, 2, const_cast<LPWSTR>(date_str.c_str()));

    // Path (archive internal directory)
    auto path_str = formatArchivePath(item);
    ListView_SetItemText(hwnd_, i, 3, const_cast<LPWSTR>(path_str.c_str()));

    // Resolution (not available in FileMetadata)
    ListView_SetItemText(hwnd_, i, 4, const_cast<LPWSTR>(L"-"));
}

std::wstring FileListView::formatSize(uint64_t size) {
    if (size < 1024) {
        return std::format(L"{} B", size);
    } else if (size < 1024 * 1024) {
        return std::format(L"{:.1f} KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        return std::format(L"{:.1f} MB", size / (1024.0 * 1024.0));
    } else {
        return std::format(L"{:.2f} GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

std::wstring FileListView::formatDate(const std::chrono::system_clock::time_point& time) {
    auto tt = std::chrono::system_clock::to_time_t(time);

    std::tm tm_buf;
    localtime_s(&tm_buf, &tt);

    wchar_t buf[64];
    std::wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%Y-%m-%d %H:%M", &tm_buf);
    return buf;
}

std::wstring FileListView::formatResolution(uint32_t width, uint32_t height) {
    return std::format(L"{}x{}", width, height);
}

std::wstring FileListView::formatArchivePath(const fs::FileMetadata& item) {
    if (!item.is_in_archive()) {
        return L"-";
    }
    auto parent = std::filesystem::path(item.virtual_path->internal_path()).parent_path();
    auto result = parent.wstring();
    return result.empty() ? L"-" : result;
}

}  // namespace nive::ui
