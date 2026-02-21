/// @file directory_tree.hpp
/// @brief Directory tree view component

#pragma once

#include <Windows.h>

#include <CommCtrl.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nive::archive {
class ArchiveManager;
}

namespace nive::ui {
class DropTarget;
}

namespace nive::ui {

/// @brief Directory tree view component
///
/// Wraps a Win32 TreeView control for directory navigation.
class DirectoryTree {
public:
    using SelectionCallback = std::function<void(const std::filesystem::path&)>;
    using FileDropCallback =
        std::function<void(const std::vector<std::filesystem::path>& files,
                           const std::filesystem::path& dest_path, DWORD effect)>;

    DirectoryTree();
    ~DirectoryTree();

    DirectoryTree(const DirectoryTree&) = delete;
    DirectoryTree& operator=(const DirectoryTree&) = delete;

    /// @brief Create the tree view control
    /// @param parent Parent window handle
    /// @param hInstance Application instance
    /// @param id Control ID
    /// @return true if created successfully
    bool create(HWND parent, HINSTANCE hInstance, int id);

    /// @brief Get window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Set position and size
    void setBounds(int x, int y, int width, int height);

    /// @brief Initialize tree with drives
    void initialize();

    /// @brief Initialize tree with drives and network shares
    /// @param network_shares List of UNC paths to display as root nodes
    void initialize(const std::vector<std::string>& network_shares);

    /// @brief Set archive manager for archive file detection
    /// @param archive Archive manager instance (may be nullptr)
    void setArchiveManager(archive::ArchiveManager* archive);

    /// @brief Select and expand path
    void selectPath(const std::filesystem::path& path);

    /// @brief Get currently selected path
    [[nodiscard]] std::filesystem::path selectedPath() const;

    /// @brief Refresh a specific path
    void refreshPath(const std::filesystem::path& path);

    /// @brief Update network shares with diff (add new, remove deleted)
    /// @param new_shares Updated list of UNC paths
    void updateNetworkShares(const std::vector<std::string>& new_shares);

    /// @brief Set selection callback
    void onSelectionChanged(SelectionCallback callback) {
        selection_callback_ = std::move(callback);
    }

    /// @brief Set file drop callback
    void onFileDrop(FileDropCallback callback) { file_drop_callback_ = std::move(callback); }

    /// @brief Enable or disable drop target
    /// @param enable True to enable, false to disable
    void enableDropTarget(bool enable);

    /// @brief Handle notification messages
    /// @return LRESULT to propagate (e.g. NM_CUSTOMDRAW return values)
    LRESULT handleNotify(NMHDR* nmhdr);

private:
    HTREEITEM addItem(HTREEITEM parent, const std::wstring& text, const std::filesystem::path& path,
                      bool has_children);
    HTREEITEM addItemWithIcon(HTREEITEM parent, const std::wstring& text,
                              const std::filesystem::path& path, bool has_children, int icon);
    void expandItem(HTREEITEM item);
    void populateChildren(HTREEITEM parent, const std::filesystem::path& path);
    HTREEITEM findItem(HTREEITEM start, const std::filesystem::path& path);
    void addNetworkShares(const std::vector<std::string>& shares);

    std::filesystem::path getPathAtPoint(POINT screen_pt);
    void setupDropTarget();

    // Hover highlight support
    static LRESULT CALLBACK subclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR subclass_id, DWORD_PTR ref_data);
    void updateHotItem(HTREEITEM new_item);

    HWND hwnd_ = nullptr;
    HIMAGELIST image_list_ = nullptr;
    HTREEITEM hot_item_ = nullptr;
    HBRUSH hover_brush_ = nullptr;

    std::unordered_map<HTREEITEM, std::filesystem::path> item_paths_;
    std::unordered_set<HTREEITEM> archive_items_;
    std::unordered_map<std::string, HTREEITEM> network_items_;
    SelectionCallback selection_callback_;
    FileDropCallback file_drop_callback_;

    archive::ArchiveManager* archive_ = nullptr;

    // Drop target support
    DropTarget* drop_target_ = nullptr;

    int icon_folder_ = 0;
    int icon_folder_open_ = 0;
    int icon_drive_ = 0;
    int icon_network_ = 0;
    int icon_archive_ = 0;
};

}  // namespace nive::ui
