/// @file directory_tree.cpp
/// @brief Directory tree view implementation

#include "directory_tree.hpp"

#include <shellapi.h>
#include <windowsx.h>

#include <CommCtrl.h>

#include "core/archive/archive_manager.hpp"
#include "core/fs/directory.hpp"
#include "core/fs/file_metadata.hpp"
#include "dnd/drop_target.hpp"

namespace nive::ui {

DirectoryTree::DirectoryTree() = default;

DirectoryTree::~DirectoryTree() {
    if (hwnd_) {
        RemoveWindowSubclass(hwnd_, subclassProc, 0);
    }
    if (hover_brush_) {
        DeleteObject(hover_brush_);
    }
    if (drop_target_) {
        drop_target_->revokeTarget();
        drop_target_->Release();
        drop_target_ = nullptr;
    }
    if (image_list_) {
        ImageList_Destroy(image_list_);
    }
}

bool DirectoryTree::create(HWND parent, HINSTANCE hInstance, int id) {
    // Note: We don't use TVS_DISABLEDRAGDROP to allow receiving drops
    hwnd_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS |
                                TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                            0, 0, 100, 100, parent,
                            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)), hInstance, nullptr);

    if (!hwnd_) {
        return false;
    }

    // Create image list with shell icons
    image_list_ = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 10, 10);
    if (image_list_) {
        SHFILEINFOW sfi = {};
        SHSTOCKICONINFO ssii = {sizeof(ssii)};

        // Folder icon (modern Explorer style)
        if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &ssii))) {
            icon_folder_ = ImageList_AddIcon(image_list_, ssii.hIcon);
            DestroyIcon(ssii.hIcon);
        }

        // Open folder icon (modern Explorer style)
        ssii.cbSize = sizeof(ssii);
        if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDEROPEN, SHGSI_ICON | SHGSI_SMALLICON, &ssii))) {
            icon_folder_open_ = ImageList_AddIcon(image_list_, ssii.hIcon);
            DestroyIcon(ssii.hIcon);
        }

        // Drive icon (actual drive icon from shell)
        SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);
        if (sfi.hIcon) {
            icon_drive_ = ImageList_AddIcon(image_list_, sfi.hIcon);
            DestroyIcon(sfi.hIcon);
        }

        // Network/share icon (modern server share style)
        ssii.cbSize = sizeof(ssii);
        if (SUCCEEDED(SHGetStockIconInfo(SIID_SERVERSHARE, SHGSI_ICON | SHGSI_SMALLICON, &ssii))) {
            icon_network_ = ImageList_AddIcon(image_list_, ssii.hIcon);
            DestroyIcon(ssii.hIcon);
        }

        // Archive icon (use .zip file icon)
        SHGetFileInfoW(L".zip", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                       SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
        if (sfi.hIcon) {
            icon_archive_ = ImageList_AddIcon(image_list_, sfi.hIcon);
            DestroyIcon(sfi.hIcon);
        }

        TreeView_SetImageList(hwnd_, image_list_, TVSIL_NORMAL);
    }

    // Subclass for hover tracking
    SetWindowSubclass(hwnd_, subclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
    hover_brush_ = CreateSolidBrush(RGB(0xE5, 0xE5, 0xE5));

    return true;
}

void DirectoryTree::setBounds(int x, int y, int width, int height) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

void DirectoryTree::initialize() {
    initialize({});
}

void DirectoryTree::initialize(const std::vector<std::string>& network_shares) {
    TreeView_DeleteAllItems(hwnd_);
    item_paths_.clear();
    archive_items_.clear();

    // Add drives
    auto drives = fs::getDrives();
    for (const auto& drive : drives) {
        // Get volume label
        wchar_t volume_name[MAX_PATH] = {};
        GetVolumeInformationW(drive.c_str(), volume_name, MAX_PATH, nullptr, nullptr, nullptr,
                              nullptr, 0);

        std::wstring label = drive.wstring();
        if (volume_name[0] != L'\0') {
            label = std::wstring(volume_name) + L" (" + drive.wstring().substr(0, 2) + L")";
        } else {
            label = L"Local Disk (" + drive.wstring().substr(0, 2) + L")";
        }

        addItem(TVI_ROOT, label, drive, true);
    }

    // Add network shares
    addNetworkShares(network_shares);
}

void DirectoryTree::setArchiveManager(archive::ArchiveManager* archive) {
    archive_ = archive;
}

void DirectoryTree::addNetworkShares(const std::vector<std::string>& shares) {
    for (const auto& share : shares) {
        std::filesystem::path share_path(share);
        std::wstring label = share_path.wstring();

        HTREEITEM item = addItemWithIcon(TVI_ROOT, label, share_path, true, icon_network_);
        if (item) {
            network_items_[share] = item;
        }
    }
}

void DirectoryTree::updateNetworkShares(const std::vector<std::string>& new_shares) {
    std::unordered_set<std::string> new_set(new_shares.begin(), new_shares.end());
    std::unordered_set<std::string> old_set;
    for (const auto& [key, _] : network_items_) {
        old_set.insert(key);
    }

    // Remove shares that are no longer in the list
    for (const auto& old_share : old_set) {
        if (!new_set.contains(old_share)) {
            auto it = network_items_.find(old_share);
            if (it != network_items_.end()) {
                // Clean up child item_paths_ entries
                std::function<void(HTREEITEM)> removeChildren = [&](HTREEITEM parent) {
                    HTREEITEM child = TreeView_GetChild(hwnd_, parent);
                    while (child) {
                        HTREEITEM next = TreeView_GetNextSibling(hwnd_, child);
                        removeChildren(child);
                        item_paths_.erase(child);
                        archive_items_.erase(child);
                        child = next;
                    }
                };
                removeChildren(it->second);
                item_paths_.erase(it->second);
                TreeView_DeleteItem(hwnd_, it->second);
                network_items_.erase(it);
            }
        }
    }

    // Add new shares
    for (const auto& new_share : new_shares) {
        if (!old_set.contains(new_share)) {
            std::filesystem::path share_path(new_share);
            std::wstring label = share_path.wstring();
            HTREEITEM item = addItemWithIcon(TVI_ROOT, label, share_path, true, icon_network_);
            if (item) {
                network_items_[new_share] = item;
            }
        }
    }
}

void DirectoryTree::selectPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    // Build path components
    std::vector<std::filesystem::path> components;
    std::filesystem::path current = path;
    while (current.has_parent_path() && current != current.root_path()) {
        components.push_back(current);
        current = current.parent_path();
    }
    components.push_back(current);  // Root

    // Reverse to go from root to target
    std::reverse(components.begin(), components.end());

    // Expand and select each level
    HTREEITEM item = TreeView_GetRoot(hwnd_);
    for (const auto& component : components) {
        item = findItem(item, component);
        if (!item) {
            break;
        }

        TreeView_Expand(hwnd_, item, TVE_EXPAND);
    }

    if (item) {
        TreeView_SelectItem(hwnd_, item);
        TreeView_EnsureVisible(hwnd_, item);
    }
}

std::filesystem::path DirectoryTree::selectedPath() const {
    HTREEITEM selected = TreeView_GetSelection(hwnd_);
    if (!selected) {
        return {};
    }

    auto it = item_paths_.find(selected);
    if (it != item_paths_.end()) {
        return it->second;
    }
    return {};
}

void DirectoryTree::refreshPath(const std::filesystem::path& path) {
    HTREEITEM item = findItem(TreeView_GetRoot(hwnd_), path);
    if (item) {
        // Delete children and re-populate
        HTREEITEM child = TreeView_GetChild(hwnd_, item);
        while (child) {
            HTREEITEM next = TreeView_GetNextSibling(hwnd_, child);
            item_paths_.erase(child);
            TreeView_DeleteItem(hwnd_, child);
            child = next;
        }

        populateChildren(item, path);
    }
}

LRESULT CALLBACK DirectoryTree::subclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                              UINT_PTR /*subclass_id*/, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<DirectoryTree*>(ref_data);

    switch (msg) {
    case WM_MOUSEMOVE: {
        TVHITTESTINFO ht = {};
        ht.pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        HTREEITEM hit_item = TreeView_HitTest(hwnd, &ht);
        if (!(ht.flags & (TVHT_ONITEM | TVHT_ONITEMBUTTON | TVHT_ONITEMINDENT))) {
            hit_item = nullptr;
        }
        self->updateHotItem(hit_item);

        // Register for WM_MOUSELEAVE
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE:
        self->updateHotItem(nullptr);
        break;
    case WM_LBUTTONDOWN:
        // Clear hover highlight before selection processing to prevent
        // flash of hover background + selection text color
        self->updateHotItem(nullptr);
        break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void DirectoryTree::updateHotItem(HTREEITEM new_item) {
    if (hot_item_ == new_item) {
        return;
    }

    RECT client_rc;
    GetClientRect(hwnd_, &client_rc);

    // Invalidate old hot item
    if (hot_item_) {
        RECT rc;
        if (TreeView_GetItemRect(hwnd_, hot_item_, &rc, FALSE)) {
            rc.left = 0;
            rc.right = client_rc.right;
            InvalidateRect(hwnd_, &rc, TRUE);
        }
    }

    hot_item_ = new_item;

    // Invalidate new hot item
    if (hot_item_) {
        RECT rc;
        if (TreeView_GetItemRect(hwnd_, hot_item_, &rc, FALSE)) {
            rc.left = 0;
            rc.right = client_rc.right;
            InvalidateRect(hwnd_, &rc, TRUE);
        }
    }
}

LRESULT DirectoryTree::handleNotify(NMHDR* nmhdr) {
    switch (nmhdr->code) {
    case TVN_SELCHANGEDW: {
        auto* nmtv = reinterpret_cast<NMTREEVIEWW*>(nmhdr);
        auto it = item_paths_.find(nmtv->itemNew.hItem);
        if (it != item_paths_.end() && selection_callback_) {
            selection_callback_(it->second);
        }
        return 0;
    }

    case TVN_ITEMEXPANDINGW: {
        auto* nmtv = reinterpret_cast<NMTREEVIEWW*>(nmhdr);
        if (nmtv->action == TVE_EXPAND) {
            expandItem(nmtv->itemNew.hItem);
        }
        return 0;
    }

    case NM_CUSTOMDRAW: {
        auto* nmcd = reinterpret_cast<NMTVCUSTOMDRAW*>(nmhdr);

        switch (nmcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT: {
            auto item = reinterpret_cast<HTREEITEM>(nmcd->nmcd.dwItemSpec);
            bool is_hot = (item == hot_item_ && hot_item_ != nullptr);
            bool is_selected = (nmcd->nmcd.uItemState & CDIS_SELECTED) != 0
                               || (item == TreeView_GetSelection(hwnd_));

            if (is_hot && !is_selected) {
                // Draw hover background across full row width
                RECT row_rect;
                TreeView_GetItemRect(hwnd_, item, &row_rect, FALSE);
                RECT client_rc;
                GetClientRect(hwnd_, &client_rc);
                row_rect.left = 0;
                row_rect.right = client_rc.right;

                FillRect(nmcd->nmcd.hdc, &row_rect, hover_brush_);
                nmcd->clrTextBk = RGB(0xE5, 0xE5, 0xE5);
                return CDRF_NEWFONT;
            }
            return CDRF_DODEFAULT;
        }
        }
        return CDRF_DODEFAULT;
    }

    default:
        return 0;
    }
}

HTREEITEM DirectoryTree::addItem(HTREEITEM parent, const std::wstring& text,
                                 const std::filesystem::path& path, bool has_children) {
    // Select icon based on path type
    int icon;
    int selected_icon;
    if (parent == TVI_ROOT) {
        icon = icon_drive_;
        selected_icon = icon_drive_;
    } else {
        icon = icon_folder_;
        selected_icon = icon_folder_open_;
    }

    return addItemWithIcon(parent, text, path, has_children, icon);
}

HTREEITEM DirectoryTree::addItemWithIcon(HTREEITEM parent, const std::wstring& text,
                                         const std::filesystem::path& path, bool has_children,
                                         int icon) {
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    tvis.item.pszText = const_cast<wchar_t*>(text.c_str());
    tvis.item.cChildren = has_children ? 1 : 0;
    tvis.item.iImage = icon;
    // For folders, use open folder icon when selected; otherwise same icon
    tvis.item.iSelectedImage = (icon == icon_folder_) ? icon_folder_open_ : icon;

    HTREEITEM item = TreeView_InsertItem(hwnd_, &tvis);
    if (item) {
        item_paths_[item] = path;
    }
    return item;
}

void DirectoryTree::expandItem(HTREEITEM item) {
    // Check if already populated (has real children)
    HTREEITEM child = TreeView_GetChild(hwnd_, item);
    if (child) {
        // Check if it's a dummy item
        auto it = item_paths_.find(child);
        if (it != item_paths_.end()) {
            return;  // Already populated
        }
    }

    auto it = item_paths_.find(item);
    if (it == item_paths_.end()) {
        return;
    }

    populateChildren(item, it->second);
}

void DirectoryTree::populateChildren(HTREEITEM parent, const std::filesystem::path& path) {
    // Remove dummy child if any
    HTREEITEM dummy = TreeView_GetChild(hwnd_, parent);
    if (dummy) {
        auto it = item_paths_.find(dummy);
        if (it == item_paths_.end()) {
            TreeView_DeleteItem(hwnd_, dummy);
        }
    }

    // Get subdirectories
    auto result = fs::getSubdirectories(path, false);
    bool has_any_children = result && !result->empty();

    // Check for archive files
    std::vector<std::filesystem::path> archives;
    if (archive_ && archive_->isAvailable()) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (!ec && entry.is_regular_file() && archive_->isArchive(entry.path())) {
                archives.push_back(entry.path());
            }
        }
        has_any_children = has_any_children || !archives.empty();
    }

    if (!has_any_children) {
        // No children - update item
        TVITEMW tvi = {};
        tvi.hItem = parent;
        tvi.mask = TVIF_CHILDREN;
        tvi.cChildren = 0;
        TreeView_SetItem(hwnd_, &tvi);
        return;
    }

    // Add subdirectories
    if (result) {
        for (const auto& subdir : *result) {
            bool has_children = !fs::isDirectoryEmpty(subdir);
            addItem(parent, subdir.filename().wstring(), subdir, has_children);
        }
    }

    // Add archive files (non-expandable leaf nodes)
    for (const auto& archive_path : archives) {
        auto name = archive_path.filename().wstring();
        HTREEITEM item = addItemWithIcon(parent, name, archive_path, false, icon_archive_);
        if (item) {
            archive_items_.insert(item);
        }
    }
}

HTREEITEM DirectoryTree::findItem(HTREEITEM start, const std::filesystem::path& path) {
    HTREEITEM item = start;

    while (item) {
        auto it = item_paths_.find(item);
        if (it != item_paths_.end()) {
            // Check if paths match (case-insensitive on Windows)
            std::wstring item_path = it->second.wstring();
            std::wstring target_path = path.wstring();

            // Normalize: remove trailing backslashes for comparison
            while (!item_path.empty() && (item_path.back() == L'\\' || item_path.back() == L'/')) {
                item_path.pop_back();
            }
            while (!target_path.empty() &&
                   (target_path.back() == L'\\' || target_path.back() == L'/')) {
                target_path.pop_back();
            }

            if (_wcsicmp(item_path.c_str(), target_path.c_str()) == 0) {
                return item;
            }

            // Check if target is under this item
            if (target_path.length() > item_path.length() &&
                _wcsnicmp(item_path.c_str(), target_path.c_str(), item_path.length()) == 0) {
                // Target is a subdirectory - search children
                HTREEITEM child = TreeView_GetChild(hwnd_, item);
                if (child) {
                    HTREEITEM found = findItem(child, path);
                    if (found) {
                        return found;
                    }
                }
            }
        }

        item = TreeView_GetNextSibling(hwnd_, item);
    }

    return nullptr;
}

std::filesystem::path DirectoryTree::getPathAtPoint(POINT screen_pt) {
    // Convert screen coordinates to client coordinates
    POINT client_pt = screen_pt;
    ScreenToClient(hwnd_, &client_pt);

    // Hit test
    TVHITTESTINFO ht = {};
    ht.pt = client_pt;
    HTREEITEM item = TreeView_HitTest(hwnd_, &ht);

    if (!item || !(ht.flags & (TVHT_ONITEM | TVHT_ONITEMBUTTON | TVHT_ONITEMINDENT |
                               TVHT_ONITEMLABEL | TVHT_ONITEMICON))) {
        return {};
    }

    // Get path for item
    auto it = item_paths_.find(item);
    if (it == item_paths_.end()) {
        return {};
    }

    // Don't allow drop on archive items
    if (archive_items_.count(item) > 0) {
        return {};
    }

    return it->second;
}

void DirectoryTree::setupDropTarget() {
    if (drop_target_) {
        return;  // Already set up
    }

    auto on_drop = [this](const std::vector<std::filesystem::path>& files,
                          const std::filesystem::path& dest_path, DWORD effect) {
        if (file_drop_callback_) {
            file_drop_callback_(files, dest_path, effect);
        }
    };

    auto get_drop_path = [this](POINT screen_pt) -> std::filesystem::path {
        // Update hover highlight during drag
        POINT client_pt = screen_pt;
        ScreenToClient(hwnd_, &client_pt);
        TVHITTESTINFO ht = {};
        ht.pt = client_pt;
        HTREEITEM hit_item = TreeView_HitTest(hwnd_, &ht);
        if (!(ht.flags & (TVHT_ONITEM | TVHT_ONITEMBUTTON | TVHT_ONITEMINDENT))) {
            hit_item = nullptr;
        }
        updateHotItem(hit_item);

        return getPathAtPoint(screen_pt);
    };

    drop_target_ = new DropTarget(hwnd_, on_drop, get_drop_path);
    drop_target_->onDragEnd([this]() { updateHotItem(nullptr); });
    drop_target_->registerTarget();
}

void DirectoryTree::enableDropTarget(bool enable) {
    if (enable) {
        setupDropTarget();
    } else if (drop_target_) {
        drop_target_->revokeTarget();
        drop_target_->Release();
        drop_target_ = nullptr;
    }
}

}  // namespace nive::ui
