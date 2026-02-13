/// @file listbox.cpp
/// @brief Implementation of D2D list box component

#include "listbox.hpp"

#include <algorithm>

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

const std::wstring D2DListBox::kEmptyString;

D2DListBox::D2DListBox() = default;

void D2DListBox::addItem(const std::wstring& text) {
    items_.push_back(text);
    invalidate();
}

void D2DListBox::addItems(const std::vector<std::wstring>& items) {
    items_.insert(items_.end(), items.begin(), items.end());
    invalidate();
}

void D2DListBox::insertItem(size_t index, const std::wstring& text) {
    if (index >= items_.size()) {
        items_.push_back(text);
    } else {
        items_.insert(items_.begin() + static_cast<ptrdiff_t>(index), text);
    }

    // Adjust selection if needed
    if (selected_index_ >= 0 && static_cast<size_t>(selected_index_) >= index) {
        selected_index_++;
    }

    invalidate();
}

void D2DListBox::removeItem(size_t index) {
    if (index >= items_.size()) {
        return;
    }

    items_.erase(items_.begin() + static_cast<ptrdiff_t>(index));

    // Adjust selection
    if (selected_index_ >= 0) {
        if (static_cast<size_t>(selected_index_) == index) {
            // Selected item was removed
            if (selected_index_ >= static_cast<int>(items_.size())) {
                selected_index_ = static_cast<int>(items_.size()) - 1;
            }
            if (on_selection_changed_ && selected_index_ >= 0) {
                on_selection_changed_(selected_index_);
            }
        } else if (static_cast<size_t>(selected_index_) > index) {
            selected_index_--;
        }
    }

    clampScrollOffset();
    invalidate();
}

void D2DListBox::clearItems() {
    items_.clear();
    selected_index_ = -1;
    hovered_index_ = -1;
    scroll_offset_ = 0.0f;
    invalidate();
}

const std::wstring& D2DListBox::itemAt(size_t index) const {
    if (index < items_.size()) {
        return items_[index];
    }
    return kEmptyString;
}

void D2DListBox::setSelectedIndex(int index) {
    if (index < -1)
        index = -1;
    if (index >= static_cast<int>(items_.size()))
        index = static_cast<int>(items_.size()) - 1;

    if (selected_index_ != index) {
        selected_index_ = index;
        invalidate();

        if (on_selection_changed_) {
            on_selection_changed_(selected_index_);
        }
    }
}

std::wstring D2DListBox::selectedText() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return items_[selected_index_];
    }
    return {};
}

void D2DListBox::setScrollOffset(float offset) {
    scroll_offset_ = offset;
    clampScrollOffset();
    invalidate();
}

void D2DListBox::ensureSelectedVisible() {
    if (selected_index_ < 0) {
        return;
    }

    Rect content = contentRect();
    float item_height = Style::itemHeight();
    float item_top = static_cast<float>(selected_index_) * item_height;
    float item_bottom = item_top + item_height;

    if (item_top < scroll_offset_) {
        scroll_offset_ = item_top;
    } else if (item_bottom > scroll_offset_ + content.height) {
        scroll_offset_ = item_bottom - content.height;
    }

    clampScrollOffset();
    invalidate();
}

Size D2DListBox::measure(const Size& available_size) {
    // ListBox can be any size, prefer reasonable defaults
    float width = available_size.width > 0 ? available_size.width : 200.0f;
    float height = available_size.height > 0 ? available_size.height : 150.0f;

    desired_size_ = Size{width, height};
    return desired_size_;
}

void D2DListBox::render(ID2D1RenderTarget* rt) {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) {
        return;
    }

    // Determine colors based on state
    Color border_color =
        enabled_ ? (focused_ ? Style::focusedBorder() : Style::border()) : Style::disabledBorder();

    Color bg_color = enabled_ ? Style::background() : Style::disabledBackground();

    background_brush_->SetColor(bg_color.toD2D());
    border_brush_->SetColor(border_color.toD2D());

    float border_width = Style::borderWidth();
    float radius = Style::borderRadius();

    D2D1_ROUNDED_RECT rounded_rect = {
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
        radius, radius};

    // Draw background
    rt->FillRoundedRectangle(rounded_rect, background_brush_.Get());

    // Draw border
    rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), border_width);

    // Clip to content area for items
    Rect content = contentRect();
    D2D1_RECT_F clip_rect =
        D2D1::RectF(content.x, content.y, content.x + content.width, content.y + content.height);

    rt->PushAxisAlignedClip(clip_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Draw items
    float item_height = Style::itemHeight();
    float item_padding = Style::itemPadding();
    float y = content.y - scroll_offset_;

    Color text_color = enabled_ ? Style::textColor() : Style::disabledTextColor();
    text_brush_->SetColor(text_color.toD2D());

    for (size_t i = 0; i < items_.size(); ++i) {
        // Skip items above visible area
        if (y + item_height <= content.y) {
            y += item_height;
            continue;
        }

        // Stop if below visible area
        if (y >= content.y + content.height) {
            break;
        }

        D2D1_RECT_F item_rect =
            D2D1::RectF(content.x, y, content.x + content.width, y + item_height);

        // Draw item background
        if (static_cast<int>(i) == selected_index_) {
            rt->FillRectangle(item_rect, selected_item_brush_.Get());
        } else if (static_cast<int>(i) == hovered_index_ && enabled_) {
            rt->FillRectangle(item_rect, item_hover_brush_.Get());
        }

        // Draw item text
        ComPtr<IDWriteTextLayout> item_layout;
        D2DFactory::instance().dwriteFactory()->CreateTextLayout(
            items_[i].c_str(), static_cast<UINT32>(items_[i].length()), text_format_.Get(),
            content.width - item_padding * 2, item_height, &item_layout);

        if (item_layout) {
            ID2D1SolidColorBrush* brush = (static_cast<int>(i) == selected_index_)
                                              ? selected_item_text_brush_.Get()
                                              : text_brush_.Get();

            rt->DrawTextLayout(D2D1::Point2F(content.x + item_padding, y), item_layout.Get(),
                               brush);
        }

        y += item_height;
    }

    rt->PopAxisAlignedClip();

    // Draw scrollbar if needed
    if (isScrollbarNeeded()) {
        renderScrollbar(rt);
    }
}

void D2DListBox::renderScrollbar(ID2D1RenderTarget* rt) {
    Rect track = scrollbarRect();
    Rect thumb = scrollThumbRect();

    // Draw track
    D2D1_RECT_F track_rect =
        D2D1::RectF(track.x, track.y, track.x + track.width, track.y + track.height);
    rt->FillRectangle(track_rect, scrollbar_track_brush_.Get());

    // Draw thumb
    Color thumb_color = (scrollbar_hovered_ || scrollbar_dragging_) ? Style::scrollbarThumbHover()
                                                                    : Style::scrollbarThumb();
    scrollbar_thumb_brush_->SetColor(thumb_color.toD2D());

    D2D1_ROUNDED_RECT thumb_rect = {D2D1::RectF(thumb.x + 2, thumb.y + 2, thumb.x + thumb.width - 2,
                                                thumb.y + thumb.height - 2),
                                    3.0f, 3.0f};
    rt->FillRoundedRectangle(thumb_rect, scrollbar_thumb_brush_.Get());
}

bool D2DListBox::onMouseEnter(const MouseEvent& event) {
    hovered_ = true;
    invalidate();
    return true;
}

bool D2DListBox::onMouseLeave(const MouseEvent& event) {
    hovered_ = false;
    hovered_index_ = -1;
    scrollbar_hovered_ = false;
    invalidate();
    return true;
}

bool D2DListBox::onMouseMove(const MouseEvent& event) {
    if (!enabled_) {
        return false;
    }

    // event.position is in local coordinates (0,0 = top-left of this component)
    // Convert to absolute coordinates for comparison with bounds-based rects
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    if (scrollbar_dragging_) {
        // Update scroll position based on drag
        float track_height = contentRect().height;
        float content_height = totalContentHeight();
        float thumb_height = scrollThumbRect().height;
        float available_height = track_height - thumb_height;

        if (available_height > 0) {
            float delta_y = (event.position.y + bounds_.y) - drag_start_y_;
            float scroll_ratio = delta_y / available_height;
            scroll_offset_ =
                drag_start_offset_ + scroll_ratio * (content_height - contentRect().height);
            clampScrollOffset();
            invalidate();
        }
        return true;
    }

    // Check scrollbar hover
    if (isScrollbarNeeded()) {
        Rect sb = scrollbarRect();
        bool over_scrollbar = abs_pos.x >= sb.x && abs_pos.x <= sb.x + sb.width &&
                              abs_pos.y >= sb.y && abs_pos.y <= sb.y + sb.height;

        if (over_scrollbar != scrollbar_hovered_) {
            scrollbar_hovered_ = over_scrollbar;
            invalidate();
        }

        if (scrollbar_hovered_) {
            hovered_index_ = -1;
            return true;
        }
    }

    // Check item hover
    int new_hovered = hitTestItem(abs_pos);
    if (new_hovered != hovered_index_) {
        hovered_index_ = new_hovered;
        invalidate();
    }

    return true;
}

bool D2DListBox::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    // event.position is in local coordinates (0,0 = top-left of this component)
    // Convert to absolute coordinates for comparison with bounds-based rects
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    // Check scrollbar click
    if (isScrollbarNeeded()) {
        Rect thumb = scrollThumbRect();

        if (abs_pos.x >= thumb.x && abs_pos.x <= thumb.x + thumb.width && abs_pos.y >= thumb.y &&
            abs_pos.y <= thumb.y + thumb.height) {
            // Start drag
            scrollbar_dragging_ = true;
            drag_start_offset_ = scroll_offset_;
            drag_start_y_ = abs_pos.y;
            return true;
        }

        Rect sb = scrollbarRect();

        if (abs_pos.x >= sb.x && abs_pos.x <= sb.x + sb.width && abs_pos.y >= sb.y &&
            abs_pos.y <= sb.y + sb.height) {
            // Page scroll
            float thumb_center = thumb.y + thumb.height / 2.0f;
            float page_size = contentRect().height;

            if (abs_pos.y < thumb_center) {
                scroll_offset_ -= page_size;
            } else {
                scroll_offset_ += page_size;
            }
            clampScrollOffset();
            invalidate();
            return true;
        }
    }

    // Item selection
    int clicked = hitTestItem(abs_pos);
    if (clicked >= 0) {
        setSelectedIndex(clicked);
    }

    return true;
}

bool D2DListBox::onMouseUp(const MouseEvent& event) {
    if (scrollbar_dragging_) {
        scrollbar_dragging_ = false;
        invalidate();
        return true;
    }
    return false;
}

bool D2DListBox::onMouseWheel(const MouseEvent& event) {
    if (!enabled_) {
        return false;
    }

    float scroll_amount = Style::itemHeight() * 3.0f;
    scroll_offset_ -= static_cast<float>(event.wheelDelta) * scroll_amount;
    clampScrollOffset();
    invalidate();
    return true;
}

bool D2DListBox::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    switch (event.keyCode) {
    case VK_UP:
        if (selected_index_ > 0) {
            setSelectedIndex(selected_index_ - 1);
            ensureSelectedVisible();
        } else if (selected_index_ < 0 && !items_.empty()) {
            setSelectedIndex(static_cast<int>(items_.size()) - 1);
            ensureSelectedVisible();
        }
        return true;

    case VK_DOWN:
        if (selected_index_ < static_cast<int>(items_.size()) - 1) {
            setSelectedIndex(selected_index_ + 1);
            ensureSelectedVisible();
        } else if (selected_index_ < 0 && !items_.empty()) {
            setSelectedIndex(0);
            ensureSelectedVisible();
        }
        return true;

    case VK_HOME:
        if (!items_.empty()) {
            setSelectedIndex(0);
            ensureSelectedVisible();
        }
        return true;

    case VK_END:
        if (!items_.empty()) {
            setSelectedIndex(static_cast<int>(items_.size()) - 1);
            ensureSelectedVisible();
        }
        return true;

    case VK_PRIOR:  // Page Up
        if (selected_index_ > 0) {
            int page_items = static_cast<int>(contentRect().height / Style::itemHeight());
            int new_index = std::max(0, selected_index_ - page_items);
            setSelectedIndex(new_index);
            ensureSelectedVisible();
        }
        return true;

    case VK_NEXT:  // Page Down
        if (!items_.empty()) {
            int page_items = static_cast<int>(contentRect().height / Style::itemHeight());
            int new_index =
                std::min(static_cast<int>(items_.size()) - 1, selected_index_ + page_items);
            setSelectedIndex(new_index);
            ensureSelectedVisible();
        }
        return true;

    case VK_RETURN:
        if (selected_index_ >= 0 && on_double_click_) {
            on_double_click_(selected_index_);
        }
        return true;
    }

    return false;
}

void D2DListBox::onFocusChanged(const FocusEvent& event) {
    invalidate();
}

void D2DListBox::createResources(DeviceResources& resources) {
    // Create text format
    D2DFactory::instance().dwriteFactory()->CreateTextFormat(
        Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);

    if (text_format_) {
        text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Create brushes
    background_brush_ = resources.createSolidBrush(Style::background());
    border_brush_ = resources.createSolidBrush(Style::border());
    text_brush_ = resources.createSolidBrush(Style::textColor());
    item_hover_brush_ = resources.createSolidBrush(Style::itemHoverBackground());
    selected_item_brush_ = resources.createSolidBrush(Style::selectedItemBackground());
    selected_item_text_brush_ = resources.createSolidBrush(Style::selectedItemTextColor());
    scrollbar_track_brush_ = resources.createSolidBrush(Style::scrollbarTrack());
    scrollbar_thumb_brush_ = resources.createSolidBrush(Style::scrollbarThumb());
}

Rect D2DListBox::contentRect() const {
    Thickness padding = Style::padding();
    // Calculate if scrollbar is needed without recursion
    float available_height = bounds_.height - padding.top - padding.bottom;
    float content_height = static_cast<float>(items_.size()) * Style::itemHeight();
    float scrollbar_width = (content_height > available_height) ? Style::scrollbarWidth() : 0.0f;

    return Rect{bounds_.x + padding.left, bounds_.y + padding.top,
                bounds_.width - padding.left - padding.right - scrollbar_width, available_height};
}

Rect D2DListBox::scrollbarRect() const {
    Thickness padding = Style::padding();
    float width = Style::scrollbarWidth();

    return Rect{bounds_.x + bounds_.width - padding.right - width, bounds_.y + padding.top, width,
                bounds_.height - padding.top - padding.bottom};
}

Rect D2DListBox::scrollThumbRect() const {
    Rect track = scrollbarRect();
    float content_height = totalContentHeight();
    float view_height = contentRect().height;

    if (content_height <= view_height) {
        return track;  // Full track if no scrolling needed
    }

    float thumb_height = std::max(20.0f, track.height * (view_height / content_height));
    float scroll_ratio = scroll_offset_ / (content_height - view_height);
    float thumb_y = track.y + scroll_ratio * (track.height - thumb_height);

    return Rect{track.x, thumb_y, track.width, thumb_height};
}

float D2DListBox::totalContentHeight() const {
    return static_cast<float>(items_.size()) * Style::itemHeight();
}

float D2DListBox::maxScrollOffset() const {
    float content_height = totalContentHeight();
    float view_height = contentRect().height;
    return std::max(0.0f, content_height - view_height);
}

int D2DListBox::hitTestItem(const Point& point) const {
    Rect content = contentRect();

    if (point.x < content.x || point.x >= content.x + content.width || point.y < content.y ||
        point.y >= content.y + content.height) {
        return -1;
    }

    float local_y = point.y - content.y + scroll_offset_;
    int index = static_cast<int>(local_y / Style::itemHeight());

    if (index >= 0 && index < static_cast<int>(items_.size())) {
        return index;
    }
    return -1;
}

bool D2DListBox::isScrollbarNeeded() const {
    return totalContentHeight() > contentRect().height;
}

void D2DListBox::clampScrollOffset() {
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, maxScrollOffset());
}

}  // namespace nive::ui::d2d
