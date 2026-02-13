/// @file file_list_view.hpp
/// @brief File list view component (Win32 ListView in details mode)

#pragma once

#include <Windows.h>

#include <CommCtrl.h>

#include <array>
#include <functional>
#include <vector>

#include "core/fs/file_metadata.hpp"

namespace nive::ui {

/// @brief File list view columns
enum class FileListColumn {
    Name,
    Size,
    Date,
    Resolution,
};

/// @brief File list view component
///
/// Win32 ListView control in report (details) mode showing file information.
class FileListView {
public:
    using ItemActivatedCallback = std::function<void(size_t index)>;
    using SelectionChangedCallback = std::function<void(const std::vector<size_t>&)>;
    using SortChangedCallback = std::function<void(FileListColumn column, bool ascending)>;
    using DeleteRequestedCallback = std::function<void(const std::vector<std::filesystem::path>&)>;

    FileListView();
    ~FileListView();

    FileListView(const FileListView&) = delete;
    FileListView& operator=(const FileListView&) = delete;

    /// @brief Create the control
    bool create(HWND parent, HINSTANCE hInstance, int id);

    /// @brief Get window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Set position and size
    void setBounds(int x, int y, int width, int height);

    /// @brief Set items to display
    void setItems(const std::vector<fs::FileMetadata>& items);

    /// @brief Get item count
    [[nodiscard]] size_t itemCount() const noexcept { return items_.size(); }

    /// @brief Get selected indices
    [[nodiscard]] std::vector<size_t> selectedIndices() const;

    /// @brief Set selection
    void setSelection(const std::vector<size_t>& indices);

    /// @brief Select single item
    void selectSingle(size_t index);

    /// @brief Clear selection
    void clearSelection();

    /// @brief Ensure item is visible
    void ensureVisible(size_t index);

    /// @brief Set sort column and direction
    void setSort(FileListColumn column, bool ascending);

    /// @brief Get column widths
    /// @return Array of column widths [Name, Size, Date, Resolution]
    [[nodiscard]] std::array<int, 4> getColumnWidths() const;

    /// @brief Set column widths
    /// @param widths Array of column widths [Name, Size, Date, Resolution]
    void setColumnWidths(const std::array<int, 4>& widths);

    /// @brief Refresh display
    void refresh();

    /// @brief Handle WM_NOTIFY messages
    bool handleNotify(NMHDR* nmhdr);

    /// @brief Set callbacks
    void onItemActivated(ItemActivatedCallback callback) {
        item_activated_callback_ = std::move(callback);
    }

    void onSelectionChanged(SelectionChangedCallback callback) {
        selection_changed_callback_ = std::move(callback);
    }

    void onSortChanged(SortChangedCallback callback) {
        sort_changed_callback_ = std::move(callback);
    }

    void onDeleteRequested(DeleteRequestedCallback callback) {
        delete_requested_callback_ = std::move(callback);
    }

    /// @brief Get file paths for selected items
    /// @return List of file paths (excludes archive entries)
    [[nodiscard]] std::vector<std::filesystem::path> selectedFilePaths() const;

private:
    void createColumns();
    void populateItems();
    void updateItem(size_t index);
    static std::wstring formatSize(uint64_t size);
    static std::wstring formatDate(const std::chrono::system_clock::time_point& time);
    static std::wstring formatResolution(uint32_t width, uint32_t height);

    HWND hwnd_ = nullptr;
    HWND header_ = nullptr;

    std::vector<fs::FileMetadata> items_;

    // Sort state
    FileListColumn sort_column_ = FileListColumn::Name;
    bool sort_ascending_ = true;

    // Callbacks
    ItemActivatedCallback item_activated_callback_;
    SelectionChangedCallback selection_changed_callback_;
    SortChangedCallback sort_changed_callback_;
    DeleteRequestedCallback delete_requested_callback_;
};

}  // namespace nive::ui
