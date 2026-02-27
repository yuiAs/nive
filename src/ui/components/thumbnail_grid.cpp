/// @file thumbnail_grid.cpp
/// @brief Thumbnail grid implementation with Direct2D rendering

#include "thumbnail_grid.hpp"

#include <ObjIdl.h>
#include <windowsx.h>

#include <algorithm>

#include "core/fs/file_operations.hpp"
#include "core/i18n/i18n.hpp"
#include "core/util/logger.hpp"
#include "core/util/string_utils.hpp"
#include "dnd/drop_source.hpp"
#include "dnd/file_data_object.hpp"
#include "ui/app.hpp"
#include "ui/d2d/core/bitmap_utils.hpp"
#include "ui/d2d/core/d2d_factory.hpp"
#include "ui/d2d/dialog/rename/d2d_rename_dialog.hpp"

namespace nive::ui {

namespace {

constexpr wchar_t kWindowClass[] = L"NiveThumbnailGrid";
bool g_class_registered = false;

d2d::Color colorFromSysColor(int index) {
    COLORREF cr = GetSysColor(index);
    return d2d::Color::fromRgb(GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

}  // namespace

ThumbnailGrid::ThumbnailGrid() = default;
ThumbnailGrid::~ThumbnailGrid() = default;

bool ThumbnailGrid::create(HWND parent, HINSTANCE hInstance, int id) {
    // Register window class once
    if (!g_class_registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // D2D handles painting
        wc.lpszClassName = kWindowClass;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        g_class_registered = true;
    }

    // No WS_VSCROLL — custom D2D scrollbar replaces the OS scrollbar
    hwnd_ = CreateWindowExW(WS_EX_CLIENTEDGE, kWindowClass, nullptr,
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 100, 100, parent,
                            reinterpret_cast<HMENU>(static_cast<intptr_t>(id)), hInstance, this);

    if (!hwnd_) {
        return false;
    }

    // Initialize D2D render target
    if (!device_resources_.setTargetWindow(hwnd_)) {
        LOG_ERROR("Failed to initialize D2D resources for ThumbnailGrid");
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    createD2DResources();
    last_resource_epoch_ = device_resources_.resourceEpoch();

    return true;
}

void ThumbnailGrid::setBounds(int x, int y, int width, int height) {
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

void ThumbnailGrid::setItems(const std::vector<fs::FileMetadata>& items, bool preserve_scroll) {
    cancelPendingInlineEdit();
    cancelInlineEdit();

    items_ = items;
    thumbnails_.clear();
    selected_.assign(items.size(), false);
    focused_index_ = SIZE_MAX;
    anchor_index_ = SIZE_MAX;
    if (!preserve_scroll) {
        scroll_pos_ = 0;
    }

    updateLayout();
    // Clamp scroll position to valid range after layout recalculation
    scroll_pos_ = std::clamp(scroll_pos_, 0, max_scroll_);
    updateScrollbar();
    requestVisibleThumbnails();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::setThumbnail(const std::filesystem::path& path, image::DecodedImage thumbnail) {
    auto key = path.wstring();
    LOG_DEBUG("setThumbnail called: path={}, valid={}, map_size={}", pathToUtf8(path),
              thumbnail.valid(), thumbnails_.size());

    ThumbnailEntry entry;

    // Create D2D bitmap immediately if render target is available
    if (device_resources_.isValid()) {
        entry.bitmap =
            d2d::createBitmapFromDecodedImage(device_resources_.renderTarget(), thumbnail);
    }
    entry.source = std::move(thumbnail);

    thumbnails_[key] = std::move(entry);

    // Invalidate the affected item rect
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].sourceIdentifier() == key) {
            RECT rc = getItemRect(i);
            LOG_DEBUG("  Found item at index {}, rect=({},{},{},{})", i, rc.left, rc.top, rc.right,
                      rc.bottom);
            InvalidateRect(hwnd_, &rc, FALSE);
            return;
        }
    }

    LOG_WARN("  Item not found in items_ list, invalidating entire window");
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::clearThumbnails() {
    thumbnails_.clear();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::setThumbnailSize(int size) {
    thumbnail_size_ = size;
    item_width_ = size + kItemPadding * 2;
    item_height_ = size + kTextHeight + kItemPadding * 2;
    updateLayout();
    updateScrollbar();
    InvalidateRect(hwnd_, nullptr, FALSE);

    // Save to settings
    App::instance().settings().thumbnails.display_size = size;
}

std::vector<size_t> ThumbnailGrid::selectedIndices() const {
    std::vector<size_t> result;
    for (size_t i = 0; i < selected_.size(); ++i) {
        if (selected_[i]) {
            result.push_back(i);
        }
    }
    return result;
}

void ThumbnailGrid::setSelection(const std::vector<size_t>& indices) {
    std::fill(selected_.begin(), selected_.end(), false);
    for (size_t i : indices) {
        if (i < selected_.size()) {
            selected_[i] = true;
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);

    if (selection_changed_callback_) {
        selection_changed_callback_(indices);
    }
}

void ThumbnailGrid::selectSingle(size_t index) {
    std::fill(selected_.begin(), selected_.end(), false);
    if (index < selected_.size()) {
        selected_[index] = true;
        focused_index_ = index;
        anchor_index_ = index;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);

    if (selection_changed_callback_) {
        selection_changed_callback_(selectedIndices());
    }
}

void ThumbnailGrid::clearSelection() {
    std::fill(selected_.begin(), selected_.end(), false);
    focused_index_ = SIZE_MAX;
    InvalidateRect(hwnd_, nullptr, FALSE);

    if (selection_changed_callback_) {
        selection_changed_callback_({});
    }
}

void ThumbnailGrid::ensureVisible(size_t index) {
    if (index >= items_.size()) {
        return;
    }

    int row = static_cast<int>(index / columns_);
    int item_top = row * item_height_;
    int item_bottom = item_top + item_height_;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    int visible_height = rc.bottom - rc.top;

    if (item_top < scroll_pos_) {
        scroll_pos_ = item_top;
    } else if (item_bottom > scroll_pos_ + visible_height) {
        scroll_pos_ = item_bottom - visible_height;
    }

    updateScrollbar();
    requestVisibleThumbnails();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::refresh() {
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK ThumbnailGrid::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ThumbnailGrid* self;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<ThumbnailGrid*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ThumbnailGrid*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT ThumbnailGrid::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        onPaint();
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOUSEWHEEL:
        onMousewheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        onLbuttondown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_MOUSEMOVE:
        onMousemove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
        return 0;

    case WM_LBUTTONUP:
        if (scrollbar_dragging_) {
            scrollbar_dragging_ = false;
            ReleaseCapture();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        if (isInlineEditing()) {
            // Forward mouse up to editbox
            d2d::MouseEvent me;
            me.position = {static_cast<float>(GET_X_LPARAM(lParam)),
                           static_cast<float>(GET_Y_LPARAM(lParam))};
            me.button = d2d::MouseButton::Left;
            inline_edit_->onMouseUp(me);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        // Apply deferred single-selection (click on already-selected item without drag)
        if (deferred_select_index_ != SIZE_MAX) {
            size_t deferred_idx = deferred_select_index_;
            selectSingle(deferred_select_index_);
            deferred_select_index_ = SIZE_MAX;

            // Start delayed inline edit timer if click was on text area
            if (!isInlineEditing() && deferred_idx < items_.size() &&
                !items_[deferred_idx].is_in_archive()) {
                D2D1_RECT_F text_rect = getTextRect(deferred_idx);
                float fx = static_cast<float>(GET_X_LPARAM(lParam));
                float fy = static_cast<float>(GET_Y_LPARAM(lParam));
                if (fx >= text_rect.left && fx <= text_rect.right && fy >= text_rect.top &&
                    fy <= text_rect.bottom) {
                    pending_inline_edit_index_ = deferred_idx;
                    SetTimer(hwnd_, kInlineEditTimerId, GetDoubleClickTime(), nullptr);
                }
            }
        }
        drag_pending_ = false;
        return 0;

    case WM_CAPTURECHANGED:
        drag_pending_ = false;
        scrollbar_dragging_ = false;
        return 0;

    case WM_RBUTTONUP:
        onRbuttonup(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        onLbuttondblclk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYDOWN:
        onKeydown(static_cast<int>(wParam));
        return 0;

    case WM_CHAR:
        if (isInlineEditing()) {
            d2d::KeyEvent ke;
            ke.character = static_cast<wchar_t>(wParam);
            inline_edit_->onChar(ke);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;

    case WM_KILLFOCUS:
        cancelPendingInlineEdit();
        cancelInlineEdit();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return 1;  // D2D handles painting

    case WM_TIMER:
        if (wParam == kScrollDebounceTimerId) {
            KillTimer(hwnd_, kScrollDebounceTimerId);
            if (thumbnail_cancel_callback_) {
                thumbnail_cancel_callback_();
            }
            requestVisibleThumbnails();
            return 0;
        }
        if (wParam == kInlineEditTimerId) {
            KillTimer(hwnd_, kInlineEditTimerId);
            size_t idx = pending_inline_edit_index_;
            pending_inline_edit_index_ = SIZE_MAX;
            // Verify conditions: still focused, selected, same item, not in archive
            if (idx < items_.size() && idx == focused_index_ && selected_[idx] &&
                !items_[idx].is_in_archive()) {
                beginInlineEdit(idx);
            }
            return 0;
        }
        return DefWindowProcW(hwnd_, msg, wParam, lParam);

    case WM_NCDESTROY:
        KillTimer(hwnd_, kScrollDebounceTimerId);
        KillTimer(hwnd_, kInlineEditTimerId);
        return DefWindowProcW(hwnd_, msg, wParam, lParam);

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wParam);
        device_resources_.setDpi(static_cast<float>(dpi), static_cast<float>(dpi));
        createD2DResources();
        recreateThumbnailBitmaps();
        last_resource_epoch_ = device_resources_.resourceEpoch();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

// --- D2D Rendering ---

void ThumbnailGrid::onPaint() {
    LOG_TRACE("onPaint called: items={}, thumbnails={}", items_.size(), thumbnails_.size());

    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);

    // Detect device lost via epoch counter
    uint32_t epoch = device_resources_.resourceEpoch();
    if (epoch != last_resource_epoch_) {
        createD2DResources();
        recreateThumbnailBitmaps();
        last_resource_epoch_ = epoch;
    }

    if (!device_resources_.beginDraw()) {
        EndPaint(hwnd_, &ps);
        return;
    }

    auto* rt = device_resources_.renderTarget();

    // Clear background
    rt->Clear(bg_color_.toD2D());

    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    float client_width = static_cast<float>(client_rect.right);
    float client_height = static_cast<float>(client_rect.bottom);

    // Clip to content area (exclude scrollbar region)
    float content_width = (max_scroll_ > 0) ? client_width - kScrollbarWidth : client_width;
    rt->PushAxisAlignedClip(D2D1::RectF(0, 0, content_width, client_height),
                            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Draw visible items
    for (size_t i = 0; i < items_.size(); ++i) {
        RECT item_rect = getItemRect(i);

        // Cull items outside visible area
        if (item_rect.bottom < 0 || item_rect.top > client_rect.bottom) {
            continue;
        }

        const auto& item = items_[i];
        bool is_selected = selected_[i];
        bool is_focused = (i == focused_index_);

        D2D1_RECT_F d2d_item = D2D1::RectF(static_cast<float>(item_rect.left),
                                            static_cast<float>(item_rect.top),
                                            static_cast<float>(item_rect.right),
                                            static_cast<float>(item_rect.bottom));

        // Selection highlight
        if (is_selected) {
            rt->FillRectangle(d2d_item, selection_brush_.Get());
        }

        // Thumbnail area
        D2D1_RECT_F thumb_area =
            D2D1::RectF(static_cast<float>(item_rect.left + kItemPadding),
                        static_cast<float>(item_rect.top + kItemPadding),
                        static_cast<float>(item_rect.right - kItemPadding),
                        static_cast<float>(item_rect.top + kItemPadding + thumbnail_size_));

        // Draw thumbnail or placeholder
        auto key = item.sourceIdentifier();
        auto thumb_it = thumbnails_.find(key);
        if (thumb_it != thumbnails_.end() && thumb_it->second.bitmap) {
            auto bitmap_size = thumb_it->second.bitmap->GetSize();
            D2D1_RECT_F fit = calculateFitRectF(thumb_area, bitmap_size.width, bitmap_size.height);
            rt->DrawBitmap(thumb_it->second.bitmap.Get(), &fit, 1.0f,
                           D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            rt->FillRectangle(thumb_area, placeholder_brush_.Get());
            if (i < 3 && item.is_image()) {
                LOG_TRACE("  Item {} thumbnail not found in map", i);
            }
        }

        // Filename text
        D2D1_RECT_F text_rect = getTextRect(i);

        if (isInlineEditing() && i == inline_edit_index_) {
            // Render inline edit box instead of static text
            inline_edit_->arrange(d2d::Rect::fromD2D(text_rect));
            inline_edit_->render(rt);
        } else {
            auto* text_color_brush = is_selected ? selection_text_brush_.Get() : text_brush_.Get();
            rt->DrawText(item.name.c_str(), static_cast<UINT32>(item.name.length()),
                         text_format_.Get(), text_rect, text_color_brush,
                         D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // Focus rectangle
        if (is_focused && GetFocus() == hwnd_) {
            D2D1_RECT_F focus_rect =
                D2D1::RectF(d2d_item.left + 2, d2d_item.top + 2, d2d_item.right - 2,
                            d2d_item.bottom - 2);
            rt->DrawRectangle(focus_rect, text_brush_.Get(), 1.0f);
        }
    }

    rt->PopAxisAlignedClip();

    // Custom scrollbar
    if (max_scroll_ > 0) {
        renderScrollbar(rt);
    }

    device_resources_.endDraw();
    EndPaint(hwnd_, &ps);
}

void ThumbnailGrid::onSize(int width, int height) {
    if (width > 0 && height > 0) {
        device_resources_.resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
    updateLayout();
    updateScrollbar();
    requestVisibleThumbnails();
}

void ThumbnailGrid::onMousewheel(int delta) {
    cancelPendingInlineEdit();
    cancelInlineEdit();

    // Ctrl+Wheel: adjust thumbnail display size
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        constexpr int kMinDisplaySize = 64;
        constexpr int kMaxDisplaySize = 512;
        constexpr int kSizeStep = 16;

        int new_size = thumbnail_size_;
        if (delta > 0) {
            new_size = (std::min)(thumbnail_size_ + kSizeStep, kMaxDisplaySize);
        } else {
            new_size = (std::max)(thumbnail_size_ - kSizeStep, kMinDisplaySize);
        }

        if (new_size != thumbnail_size_) {
            setThumbnailSize(new_size);
        }
        return;
    }

    // Normal wheel: scroll (3 half-rows per notch, matching original behavior)
    int notches = delta / WHEEL_DELTA;
    int scroll_amount = notches * (item_height_ * 3 / 2);
    scroll_pos_ = std::clamp(scroll_pos_ - scroll_amount, 0, max_scroll_);

    InvalidateRect(hwnd_, nullptr, FALSE);
    scheduleScrollThumbnailRequest();
}

void ThumbnailGrid::onLbuttondown(int x, int y, WPARAM keys) {
    cancelPendingInlineEdit();

    // Handle inline edit: click inside editbox forwards, click outside cancels
    if (isInlineEditing()) {
        D2D1_RECT_F text_rect = getTextRect(inline_edit_index_);
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        if (fx >= text_rect.left && fx <= text_rect.right && fy >= text_rect.top &&
            fy <= text_rect.bottom) {
            // Forward to editbox for caret positioning
            d2d::MouseEvent me;
            me.position = {fx, fy};
            me.button = d2d::MouseButton::Left;
            inline_edit_->onMouseDown(me);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        // Click outside edit area — cancel
        cancelInlineEdit();
    }

    // Check scrollbar interaction first
    if (max_scroll_ > 0) {
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        d2d::Rect thumb = scrollbarThumbRect();
        d2d::Rect track = scrollbarTrackRect();

        if (thumb.contains(fx, fy)) {
            // Start scrollbar thumb drag
            cancelInlineEdit();
            scrollbar_dragging_ = true;
            scrollbar_drag_start_offset_ = static_cast<float>(scroll_pos_);
            scrollbar_drag_start_y_ = fy;
            SetCapture(hwnd_);
            return;
        }

        if (track.contains(fx, fy)) {
            // Page scroll (click on track above/below thumb)
            float thumb_center = thumb.y + thumb.height / 2.0f;
            RECT rc;
            GetClientRect(hwnd_, &rc);
            int page_size = rc.bottom - rc.top;

            if (fy < thumb_center) {
                scroll_pos_ -= page_size;
            } else {
                scroll_pos_ += page_size;
            }
            scroll_pos_ = std::clamp(scroll_pos_, 0, max_scroll_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            scheduleScrollThumbnailRequest();
            return;
        }
    }

    // Item hit test and selection (existing logic)
    size_t index = hitTest(x, y);
    if (index == SIZE_MAX) {
        if (!(keys & MK_CONTROL)) {
            clearSelection();
        }
        drag_pending_ = false;
        return;
    }

    // Record drag start position
    drag_pending_ = true;
    drag_start_point_.x = x;
    drag_start_point_.y = y;
    deferred_select_index_ = SIZE_MAX;

    bool ctrl = (keys & MK_CONTROL) != 0;
    bool shift = (keys & MK_SHIFT) != 0;
    bool already_selected = (index < selected_.size() && selected_[index]);

    if (shift && anchor_index_ != SIZE_MAX) {
        // Range selection
        size_t start = (std::min)(anchor_index_, index);
        size_t end = (std::max)(anchor_index_, index);

        if (!ctrl) {
            std::fill(selected_.begin(), selected_.end(), false);
        }

        for (size_t i = start; i <= end; ++i) {
            selected_[i] = true;
        }
        focused_index_ = index;
    } else if (ctrl) {
        // Toggle selection
        selected_[index] = !selected_[index];
        focused_index_ = index;
        anchor_index_ = index;
    } else if (already_selected) {
        // Defer single-selection to button up (allows multi-select D&D)
        deferred_select_index_ = index;
        focused_index_ = index;
    } else {
        // Single selection
        selectSingle(index);
    }

    InvalidateRect(hwnd_, nullptr, FALSE);

    if (selection_changed_callback_) {
        selection_changed_callback_(selectedIndices());
    }
}

void ThumbnailGrid::onLbuttondblclk(int x, int y) {
    cancelPendingInlineEdit();
    cancelInlineEdit();
    drag_pending_ = false;  // Cancel any pending drag
    size_t index = hitTest(x, y);
    if (index != SIZE_MAX && item_activated_callback_) {
        item_activated_callback_(index);
    }
}

void ThumbnailGrid::onRbuttonup(int x, int y) {
    cancelPendingInlineEdit();
    cancelInlineEdit();
    size_t index = hitTest(x, y);
    if (index == SIZE_MAX) {
        return;
    }
    showContextMenu(x, y, index);
}

void ThumbnailGrid::showContextMenu(int x, int y, size_t index) {
    // Context menu command IDs
    constexpr UINT kCmdOpen = 1;
    constexpr UINT kCmdRename = 2;
    constexpr UINT kCmdDelete = 3;

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kCmdOpen, i18n::tr("context_menu.open").c_str());
    AppendMenuW(menu, MF_STRING, kCmdRename, i18n::tr("context_menu.rename").c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdDelete, i18n::tr("context_menu.delete").c_str());

    POINT pt = {x, y};
    ClientToScreen(hwnd_, &pt);

    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
    case kCmdOpen:
        if (item_activated_callback_) {
            item_activated_callback_(index);
        }
        break;

    case kCmdRename: {
        if (index >= items_.size() || items_[index].is_in_archive()) {
            break;
        }
        d2d::D2DRenameDialog dialog;
        auto new_name = dialog.show(hwnd_, items_[index].name);
        if (new_name && rename_requested_callback_) {
            rename_requested_callback_(index, *new_name);
        }
        break;
    }

    case kCmdDelete: {
        auto paths = selectedFilePaths();
        if (!paths.empty() && delete_requested_callback_) {
            delete_requested_callback_(paths);
        }
        break;
    }
    }
}

void ThumbnailGrid::onMousemove(int x, int y, WPARAM keys) {
    // Forward mouse move to editbox for text selection drag
    if (isInlineEditing() && (keys & MK_LBUTTON)) {
        d2d::MouseEvent me;
        me.position = {static_cast<float>(x), static_cast<float>(y)};
        me.button = d2d::MouseButton::Left;
        inline_edit_->onMouseMove(me);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    // Handle scrollbar dragging
    if (scrollbar_dragging_) {
        float fy = static_cast<float>(y);
        d2d::Rect track = scrollbarTrackRect();
        d2d::Rect thumb = scrollbarThumbRect();
        float available_height = track.height - thumb.height;

        if (available_height > 0.0f) {
            float delta_y = fy - scrollbar_drag_start_y_;
            float scroll_ratio = delta_y / available_height;
            scroll_pos_ = static_cast<int>(scrollbar_drag_start_offset_ +
                                           scroll_ratio * static_cast<float>(max_scroll_));
            scroll_pos_ = std::clamp(scroll_pos_, 0, max_scroll_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            scheduleScrollThumbnailRequest();
        }
        return;
    }

    // Scrollbar hover detection
    if (max_scroll_ > 0) {
        d2d::Rect track = scrollbarTrackRect();
        bool was_hovered = scrollbar_hovered_;
        scrollbar_hovered_ = track.contains(static_cast<float>(x), static_cast<float>(y));
        if (scrollbar_hovered_ != was_hovered) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    // Existing drag-to-select logic
    if (!drag_pending_ || !(keys & MK_LBUTTON)) {
        return;
    }

    int dx = x - drag_start_point_.x;
    int dy = y - drag_start_point_.y;

    if (abs(dx) > kDragThreshold || abs(dy) > kDragThreshold) {
        drag_pending_ = false;
        deferred_select_index_ = SIZE_MAX;  // Cancel deferred select on drag
        beginDrag();
    }
}

void ThumbnailGrid::beginDrag() {
    auto paths = selectedFilePaths();
    if (paths.empty()) {
        return;
    }

    // Notify callback
    if (drag_start_callback_) {
        drag_start_callback_(paths);
    }

    // Create OLE data object and drop source
    IDataObject* pDataObj = createFileDataObject(paths);
    IDropSource* pDropSource = new DropSource();

    // Perform drag-drop
    DWORD dwEffect = 0;
    HRESULT hr = DoDragDrop(pDataObj, pDropSource, DROPEFFECT_COPY | DROPEFFECT_MOVE, &dwEffect);

    // Release COM objects
    pDropSource->Release();
    pDataObj->Release();

    // Handle result if needed (dwEffect tells us what actually happened)
    if (hr == DRAGDROP_S_DROP && dwEffect == DROPEFFECT_MOVE) {
        // Files were moved - could refresh the view here
    }
}

std::vector<std::filesystem::path> ThumbnailGrid::selectedFilePaths() const {
    std::vector<std::filesystem::path> paths;

    for (size_t i = 0; i < items_.size(); ++i) {
        if (selected_[i] && !items_[i].is_in_archive()) {
            paths.push_back(items_[i].path);
        }
    }

    return paths;
}

void ThumbnailGrid::onKeydown(int vk) {
    if (items_.empty()) {
        return;
    }

    // Inline edit event forwarding
    if (isInlineEditing()) {
        if (vk == VK_RETURN) {
            commitInlineEdit();
            return;
        }
        if (vk == VK_ESCAPE) {
            cancelInlineEdit();
            return;
        }
        // Forward all other keys to editbox
        d2d::KeyEvent ke;
        ke.keyCode = static_cast<uint32_t>(vk);
        ke.modifiers = d2d::Modifiers::None;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            ke.modifiers = ke.modifiers | d2d::Modifiers::Shift;
        if (GetKeyState(VK_CONTROL) & 0x8000)
            ke.modifiers = ke.modifiers | d2d::Modifiers::Ctrl;
        if (GetKeyState(VK_MENU) & 0x8000)
            ke.modifiers = ke.modifiers | d2d::Modifiers::Alt;
        inline_edit_->onKeyDown(ke);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    cancelPendingInlineEdit();

    size_t new_focus = focused_index_;

    switch (vk) {
    case VK_LEFT:
        if (focused_index_ != SIZE_MAX && focused_index_ > 0) {
            new_focus = focused_index_ - 1;
        }
        break;

    case VK_RIGHT:
        if (focused_index_ != SIZE_MAX && focused_index_ < items_.size() - 1) {
            new_focus = focused_index_ + 1;
        } else if (focused_index_ == SIZE_MAX && !items_.empty()) {
            new_focus = 0;
        }
        break;

    case VK_UP:
        if (focused_index_ != SIZE_MAX && focused_index_ >= static_cast<size_t>(columns_)) {
            new_focus = focused_index_ - columns_;
        }
        break;

    case VK_DOWN:
        if (focused_index_ != SIZE_MAX) {
            size_t next = focused_index_ + columns_;
            if (next < items_.size()) {
                new_focus = next;
            }
        } else if (!items_.empty()) {
            new_focus = 0;
        }
        break;

    case VK_HOME:
        new_focus = 0;
        break;

    case VK_END:
        new_focus = items_.size() - 1;
        break;

    case VK_RETURN:
        if (focused_index_ != SIZE_MAX && item_activated_callback_) {
            item_activated_callback_(focused_index_);
        }
        return;

    case 'A':
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            std::fill(selected_.begin(), selected_.end(), true);
            InvalidateRect(hwnd_, nullptr, FALSE);
            if (selection_changed_callback_) {
                selection_changed_callback_(selectedIndices());
            }
        }
        return;

    case VK_F2:
        if (focused_index_ != SIZE_MAX && focused_index_ < items_.size() &&
            selected_[focused_index_] && !items_[focused_index_].is_in_archive()) {
            // Only allow F2 rename when exactly one item is selected
            auto sel = selectedIndices();
            if (sel.size() == 1) {
                beginInlineEdit(focused_index_);
            }
        }
        return;

    case VK_DELETE: {
        auto paths = selectedFilePaths();
        if (!paths.empty() && delete_requested_callback_) {
            delete_requested_callback_(paths);
        }
        return;
    }

    default:
        return;
    }

    if (new_focus != focused_index_ && new_focus < items_.size()) {
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (!shift && !ctrl) {
            selectSingle(new_focus);
        } else if (shift && anchor_index_ != SIZE_MAX) {
            size_t start = (std::min)(anchor_index_, new_focus);
            size_t end = (std::max)(anchor_index_, new_focus);
            std::fill(selected_.begin(), selected_.end(), false);
            for (size_t i = start; i <= end; ++i) {
                selected_[i] = true;
            }
            focused_index_ = new_focus;
        } else {
            focused_index_ = new_focus;
        }

        ensureVisible(new_focus);
        InvalidateRect(hwnd_, nullptr, FALSE);

        if (selection_changed_callback_) {
            selection_changed_callback_(selectedIndices());
        }
    }
}

// --- Layout ---

void ThumbnailGrid::updateLayout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // First pass: compute columns without scrollbar
    columns_ = (std::max)(1, width / item_width_);
    int total_rows = (static_cast<int>(items_.size()) + columns_ - 1) / columns_;
    int content_height = total_rows * item_height_;

    // If scrollbar is needed, recompute with reduced width
    if (content_height > height) {
        int available_width = width - static_cast<int>(kScrollbarWidth);
        columns_ = (std::max)(1, available_width / item_width_);
        total_rows = (static_cast<int>(items_.size()) + columns_ - 1) / columns_;
        content_height = total_rows * item_height_;
    }

    visible_rows_ = (height + item_height_ - 1) / item_height_;
    max_scroll_ = (std::max)(0, content_height - height);
}

void ThumbnailGrid::updateScrollbar() {
    // Clamp scroll position and request repaint (custom D2D scrollbar)
    scroll_pos_ = std::clamp(scroll_pos_, 0, max_scroll_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

size_t ThumbnailGrid::hitTest(int x, int y) const {
    // Exclude scrollbar area from item hit testing
    if (max_scroll_ > 0) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        if (x >= rc.right - static_cast<int>(kScrollbarWidth)) {
            return SIZE_MAX;
        }
    }

    int adjusted_y = y + scroll_pos_;

    int col = x / item_width_;
    int row = adjusted_y / item_height_;

    if (col < 0 || col >= columns_) {
        return SIZE_MAX;
    }

    size_t index = row * columns_ + col;
    if (index >= items_.size()) {
        return SIZE_MAX;
    }

    return index;
}

RECT ThumbnailGrid::getItemRect(size_t index) const {
    int col = static_cast<int>(index % columns_);
    int row = static_cast<int>(index / columns_);

    RECT rc;
    rc.left = col * item_width_;
    rc.top = row * item_height_ - scroll_pos_;
    rc.right = rc.left + item_width_;
    rc.bottom = rc.top + item_height_;

    return rc;
}

D2D1_RECT_F ThumbnailGrid::getTextRect(size_t index) const {
    RECT item_rect = getItemRect(index);
    return D2D1::RectF(static_cast<float>(item_rect.left + 4),
                       static_cast<float>(item_rect.top + kItemPadding + thumbnail_size_ + 2),
                       static_cast<float>(item_rect.right - 4),
                       static_cast<float>(item_rect.bottom));
}

void ThumbnailGrid::beginInlineEdit(size_t index) {
    if (index >= items_.size() || items_[index].is_in_archive()) {
        return;
    }
    cancelPendingInlineEdit();

    inline_edit_index_ = index;
    inline_edit_original_name_ = items_[index].name;

    if (!inline_edit_) {
        inline_edit_ = std::make_unique<d2d::D2DEditBox>();
        inline_edit_->createResources(device_resources_);
        inline_edit_->setMaxLength(255);
    }

    inline_edit_->setText(inline_edit_original_name_);

    // Select filename stem (exclude extension), matching D2DRenameDialog logic
    auto dot_pos = inline_edit_original_name_.rfind(L'.');
    if (dot_pos != std::wstring::npos && dot_pos > 0) {
        inline_edit_->setSelection(0, dot_pos);
    } else {
        inline_edit_->selectAll();
    }

    // Layout the edit box
    D2D1_RECT_F text_rect = getTextRect(index);
    d2d::Size available{text_rect.right - text_rect.left, text_rect.bottom - text_rect.top};
    inline_edit_->measure(available);
    inline_edit_->arrange(d2d::Rect::fromD2D(text_rect));
    inline_edit_->setFocused(true);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::commitInlineEdit() {
    if (!isInlineEditing()) {
        return;
    }

    auto new_name = inline_edit_->text();
    size_t index = inline_edit_index_;

    // Clear edit state first
    inline_edit_->setFocused(false);
    inline_edit_index_ = SIZE_MAX;
    inline_edit_original_name_.clear();

    // Commit only if name actually changed and is valid
    if (new_name != items_[index].name && !new_name.empty() &&
        fs::isValidFilename(new_name)) {
        if (rename_requested_callback_) {
            rename_requested_callback_(index, new_name);
        }
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::cancelInlineEdit() {
    if (!isInlineEditing()) {
        return;
    }

    inline_edit_->setFocused(false);
    inline_edit_index_ = SIZE_MAX;
    inline_edit_original_name_.clear();

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ThumbnailGrid::cancelPendingInlineEdit() {
    if (pending_inline_edit_index_ != SIZE_MAX) {
        KillTimer(hwnd_, kInlineEditTimerId);
        pending_inline_edit_index_ = SIZE_MAX;
    }
}

void ThumbnailGrid::requestVisibleThumbnails() {
    if (!thumbnail_request_callback_) {
        LOG_DEBUG("requestVisibleThumbnails: no callback set");
        return;
    }

    if (items_.empty() || columns_ <= 0 || item_height_ <= 0) {
        return;
    }

    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int client_height = client_rect.bottom - client_rect.top;

    // Compute visible row range via arithmetic instead of iterating all items
    int first_row = scroll_pos_ / item_height_;
    int last_row = (scroll_pos_ + client_height) / item_height_;
    size_t first_idx = static_cast<size_t>(first_row) * columns_;
    size_t last_idx =
        (std::min)(static_cast<size_t>(last_row + 1) * columns_, items_.size());

    LOG_DEBUG(
        "requestVisibleThumbnails: rows=[{},{}], indices=[{},{}), items={}, columns={}, "
        "scroll_pos={}",
        first_row, last_row, first_idx, last_idx, items_.size(), columns_, scroll_pos_);

    size_t requested = 0;
    for (size_t i = first_idx; i < last_idx; ++i) {
        const auto& item = items_[i];

        // Check if we already have thumbnail (use sourceIdentifier for keying)
        std::wstring key = item.sourceIdentifier();
        if (item.is_image() && thumbnails_.find(key) == thumbnails_.end()) {
            // Request using VirtualPath
            if (item.is_in_archive() && item.virtual_path) {
                thumbnail_request_callback_(*item.virtual_path);
            } else {
                thumbnail_request_callback_(archive::VirtualPath(item.path));
            }
            requested++;
        }
    }

    LOG_DEBUG("requestVisibleThumbnails: requested {} thumbnails", requested);
}

void ThumbnailGrid::scheduleScrollThumbnailRequest() {
    SetTimer(hwnd_, kScrollDebounceTimerId, kScrollDebounceMs, nullptr);
}

// --- D2D Resource Management ---

void ThumbnailGrid::createD2DResources() {
    if (!device_resources_.isValid()) {
        return;
    }

    // System colors
    bg_color_ = d2d::Color::fromRgb(0xF5F5F5);  // Match D2DDialog background
    auto selection_color = colorFromSysColor(COLOR_HIGHLIGHT);
    auto selection_text_color = colorFromSysColor(COLOR_HIGHLIGHTTEXT);
    auto text_color = colorFromSysColor(COLOR_WINDOWTEXT);

    // Brushes
    bg_brush_ = device_resources_.createSolidBrush(bg_color_);
    selection_brush_ = device_resources_.createSolidBrush(selection_color);
    selection_text_brush_ = device_resources_.createSolidBrush(selection_text_color);
    text_brush_ = device_resources_.createSolidBrush(text_color);
    placeholder_brush_ = device_resources_.createSolidBrush(d2d::Color::fromRgb(0xE0E0E0));
    scrollbar_track_brush_ = device_resources_.createSolidBrush(d2d::Color::fromRgb(0xF0F0F0));
    scrollbar_thumb_brush_ = device_resources_.createSolidBrush(d2d::Color::fromRgb(0xC0C0C0));

    // Text format (9pt Segoe UI = 12 DIPs at 96 DPI)
    text_format_ = device_resources_.createTextFormat(L"Segoe UI", 12.0f);
    if (text_format_) {
        text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

        // Ellipsis trimming for overflow
        DWRITE_TRIMMING trimming = {};
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        ComPtr<IDWriteInlineObject> ellipsis;
        d2d::D2DFactory::instance().dwriteFactory()->CreateEllipsisTrimmingSign(text_format_.Get(),
                                                                               &ellipsis);
        if (ellipsis) {
            text_format_->SetTrimming(&trimming, ellipsis.Get());
        }
    }
}

void ThumbnailGrid::discardD2DResources() {
    text_format_.Reset();
    bg_brush_.Reset();
    selection_brush_.Reset();
    selection_text_brush_.Reset();
    text_brush_.Reset();
    placeholder_brush_.Reset();
    scrollbar_track_brush_.Reset();
    scrollbar_thumb_brush_.Reset();
}

void ThumbnailGrid::recreateThumbnailBitmaps() {
    if (!device_resources_.isValid()) {
        return;
    }

    auto* rt = device_resources_.renderTarget();
    for (auto& [key, entry] : thumbnails_) {
        if (entry.source.valid()) {
            entry.bitmap = d2d::createBitmapFromDecodedImage(rt, entry.source);
        }
    }
}

// --- Custom Scrollbar ---

d2d::Rect ThumbnailGrid::scrollbarTrackRect() const {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);
    return d2d::Rect{width - kScrollbarWidth, 0.0f, kScrollbarWidth, height};
}

d2d::Rect ThumbnailGrid::scrollbarThumbRect() const {
    d2d::Rect track = scrollbarTrackRect();

    RECT rc;
    GetClientRect(hwnd_, &rc);
    float view_height = static_cast<float>(rc.bottom - rc.top);

    int total_rows = (static_cast<int>(items_.size()) + columns_ - 1) / columns_;
    float content_height = static_cast<float>(total_rows * item_height_);

    if (content_height <= view_height) {
        return track;
    }

    float thumb_height = std::max(20.0f, track.height * (view_height / content_height));
    float scroll_ratio =
        (max_scroll_ > 0) ? static_cast<float>(scroll_pos_) / static_cast<float>(max_scroll_)
                          : 0.0f;
    float thumb_y = track.y + scroll_ratio * (track.height - thumb_height);

    return d2d::Rect{track.x, thumb_y, track.width, thumb_height};
}

void ThumbnailGrid::renderScrollbar(ID2D1RenderTarget* rt) {
    d2d::Rect track = scrollbarTrackRect();
    d2d::Rect thumb = scrollbarThumbRect();

    // Track background
    rt->FillRectangle(track.toD2D(), scrollbar_track_brush_.Get());

    // Thumb with hover highlight
    auto thumb_color = (scrollbar_hovered_ || scrollbar_dragging_)
                           ? d2d::Color::fromRgb(0xA0A0A0)
                           : d2d::Color::fromRgb(0xC0C0C0);
    scrollbar_thumb_brush_->SetColor(thumb_color.toD2D());

    D2D1_ROUNDED_RECT thumb_rect = {
        D2D1::RectF(thumb.left() + 2, thumb.top() + 2, thumb.right() - 2, thumb.bottom() - 2),
        3.0f, 3.0f};
    rt->FillRoundedRectangle(thumb_rect, scrollbar_thumb_brush_.Get());
}

D2D1_RECT_F ThumbnailGrid::calculateFitRectF(const D2D1_RECT_F& container, float img_w,
                                              float img_h) const {
    float container_w = container.right - container.left;
    float container_h = container.bottom - container.top;

    if (img_w <= 0.0f || img_h <= 0.0f) {
        return container;
    }

    float scale_x = container_w / img_w;
    float scale_y = container_h / img_h;
    float scale = (std::min)(scale_x, scale_y);

    float scaled_w = img_w * scale;
    float scaled_h = img_h * scale;

    float offset_x = (container_w - scaled_w) / 2.0f;
    float offset_y = (container_h - scaled_h) / 2.0f;

    return D2D1::RectF(container.left + offset_x, container.top + offset_y,
                       container.left + offset_x + scaled_w, container.top + offset_y + scaled_h);
}

}  // namespace nive::ui
