/// @file thumbnail_grid.hpp
/// @brief Thumbnail grid component

#pragma once

#include <Windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

#include "core/archive/virtual_path.hpp"
#include "core/fs/file_metadata.hpp"
#include "core/image/decoded_image.hpp"
#include "core/util/com_ptr.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/core/types.hpp"

namespace nive::ui {

/// @brief Thumbnail grid component
///
/// Custom owner-draw control for displaying file thumbnails in a grid.
/// Uses Direct2D for GPU-accelerated rendering.
class ThumbnailGrid {
public:
    using ItemActivatedCallback = std::function<void(size_t index)>;
    using SelectionChangedCallback = std::function<void(const std::vector<size_t>&)>;
    using ThumbnailRequestCallback = std::function<void(const archive::VirtualPath&)>;
    using DragStartCallback = std::function<void(const std::vector<std::filesystem::path>&)>;
    using DeleteRequestedCallback = std::function<void(const std::vector<std::filesystem::path>&)>;
    using RenameRequestedCallback = std::function<void(size_t index, const std::wstring& new_name)>;

    ThumbnailGrid();
    ~ThumbnailGrid();

    ThumbnailGrid(const ThumbnailGrid&) = delete;
    ThumbnailGrid& operator=(const ThumbnailGrid&) = delete;

    /// @brief Create the control
    bool create(HWND parent, HINSTANCE hInstance, int id);

    /// @brief Get window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Set position and size
    void setBounds(int x, int y, int width, int height);

    /// @brief Set items to display
    void setItems(const std::vector<fs::FileMetadata>& items, bool preserve_scroll = false);

    /// @brief Set thumbnail for a file
    void setThumbnail(const std::filesystem::path& path, image::DecodedImage thumbnail);

    /// @brief Clear all thumbnails
    void clearThumbnails();

    /// @brief Get item count
    [[nodiscard]] size_t itemCount() const noexcept { return items_.size(); }

    /// @brief Get thumbnail count (loaded thumbnails)
    [[nodiscard]] size_t thumbnailCount() const noexcept { return thumbnails_.size(); }

    /// @brief Set thumbnail size
    void setThumbnailSize(int size);

    /// @brief Get thumbnail size
    [[nodiscard]] int thumbnailSize() const noexcept { return thumbnail_size_; }

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

    /// @brief Set callbacks
    void onItemActivated(ItemActivatedCallback callback) {
        item_activated_callback_ = std::move(callback);
    }

    void onSelectionChanged(SelectionChangedCallback callback) {
        selection_changed_callback_ = std::move(callback);
    }

    void onThumbnailRequest(ThumbnailRequestCallback callback) {
        thumbnail_request_callback_ = std::move(callback);
    }

    void onDragStart(DragStartCallback callback) { drag_start_callback_ = std::move(callback); }

    void onDeleteRequested(DeleteRequestedCallback callback) {
        delete_requested_callback_ = std::move(callback);
    }

    void onRenameRequested(RenameRequestedCallback callback) {
        rename_requested_callback_ = std::move(callback);
    }

    /// @brief Get file paths for selected items
    /// @return List of file paths (excludes archive entries)
    [[nodiscard]] std::vector<std::filesystem::path> selectedFilePaths() const;

    /// @brief Refresh display
    void refresh();

private:
    /// @brief Cached thumbnail with D2D bitmap and source data for device-lost recreation
    struct ThumbnailEntry {
        ComPtr<ID2D1Bitmap> bitmap;
        image::DecodedImage source;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void onPaint();
    void onSize(int width, int height);
    void onMousewheel(int delta);
    void onLbuttondown(int x, int y, WPARAM keys);
    void onLbuttondblclk(int x, int y);
    void onRbuttonup(int x, int y);
    void onMousemove(int x, int y, WPARAM keys);
    void onKeydown(int vk);
    void showContextMenu(int x, int y, size_t index);

    void beginDrag();

    void updateLayout();
    void updateScrollbar();
    size_t hitTest(int x, int y) const;
    RECT getItemRect(size_t index) const;
    void requestVisibleThumbnails();

    // D2D resource management
    void createD2DResources();
    void discardD2DResources();
    void recreateThumbnailBitmaps();

    // Custom scrollbar
    [[nodiscard]] d2d::Rect scrollbarTrackRect() const;
    [[nodiscard]] d2d::Rect scrollbarThumbRect() const;
    void renderScrollbar(ID2D1RenderTarget* rt);

    // Aspect-ratio fit calculation (float-based for D2D)
    [[nodiscard]] D2D1_RECT_F calculateFitRectF(const D2D1_RECT_F& container, float img_w,
                                                 float img_h) const;

    HWND hwnd_ = nullptr;

    std::vector<fs::FileMetadata> items_;
    std::unordered_map<std::wstring, ThumbnailEntry> thumbnails_;

    // D2D rendering
    d2d::DeviceResources device_resources_;
    uint32_t last_resource_epoch_ = 0;
    d2d::Color bg_color_;

    // D2D brushes
    ComPtr<ID2D1SolidColorBrush> bg_brush_;
    ComPtr<ID2D1SolidColorBrush> selection_brush_;
    ComPtr<ID2D1SolidColorBrush> selection_text_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1SolidColorBrush> placeholder_brush_;
    ComPtr<ID2D1SolidColorBrush> scrollbar_track_brush_;
    ComPtr<ID2D1SolidColorBrush> scrollbar_thumb_brush_;

    // DirectWrite text format
    ComPtr<IDWriteTextFormat> text_format_;

    // Layout
    int thumbnail_size_ = 128;
    int item_width_ = 140;
    int item_height_ = 160;
    int columns_ = 1;
    int visible_rows_ = 1;
    int scroll_pos_ = 0;
    int max_scroll_ = 0;

    // Selection
    std::vector<bool> selected_;
    size_t focused_index_ = SIZE_MAX;
    size_t anchor_index_ = SIZE_MAX;

    // Callbacks
    ItemActivatedCallback item_activated_callback_;
    SelectionChangedCallback selection_changed_callback_;
    ThumbnailRequestCallback thumbnail_request_callback_;
    DragStartCallback drag_start_callback_;
    DeleteRequestedCallback delete_requested_callback_;
    RenameRequestedCallback rename_requested_callback_;

    // Drag state
    bool drag_pending_ = false;
    POINT drag_start_point_ = {0, 0};
    size_t deferred_select_index_ = SIZE_MAX;  // Deferred single-select on button up
    static constexpr int kDragThreshold = 5;

    // Custom scrollbar state
    bool scrollbar_hovered_ = false;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_start_offset_ = 0.0f;
    float scrollbar_drag_start_y_ = 0.0f;
    static constexpr float kScrollbarWidth = 12.0f;

    static constexpr int kItemPadding = 8;
    static constexpr int kTextHeight = 32;
};

}  // namespace nive::ui
