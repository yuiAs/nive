/// @file tabcontrol.cpp
/// @brief Implementation of D2D tab control component

#include "tabcontrol.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

static const std::wstring kEmptyString;

D2DTabControl::D2DTabControl() = default;

int D2DTabControl::addTab(const std::wstring& title,
                          std::unique_ptr<D2DContainerComponent> content) {
    TabInfo info;
    info.title = title;
    info.content = std::move(content);

    // Set parent for event bubbling (focus requests, etc.)
    setComponentParent(info.content.get(), this);
    tabs_.push_back(std::move(info));

    int index = static_cast<int>(tabs_.size()) - 1;

    // Select first tab automatically
    if (selected_index_ < 0) {
        setSelectedIndex(0);
    }

    updateTabWidths();
    invalidate();

    return index;
}

void D2DTabControl::removeTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }

    tabs_.erase(tabs_.begin() + index);

    // Adjust selection
    if (selected_index_ >= static_cast<int>(tabs_.size())) {
        selected_index_ = static_cast<int>(tabs_.size()) - 1;
    }

    updateTabWidths();
    invalidate();
}

const std::wstring& D2DTabControl::tabTitle(int index) const {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        return tabs_[index].title;
    }
    return kEmptyString;
}

void D2DTabControl::setTabTitle(int index, const std::wstring& title) {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        tabs_[index].title = title;
        updateTabWidths();
        invalidate();
    }
}

void D2DTabControl::setSelectedIndex(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }

    if (selected_index_ != index) {
        selected_index_ = index;

        // Arrange the newly selected content
        auto& content = tabs_[selected_index_].content;
        if (content && !content_bounds_.isEmpty()) {
            Rect abs_content = content_bounds_.translated(bounds_.x, bounds_.y);
            Rect inner = abs_content.deflated(Style::borderWidth(), Style::borderWidth());
            content->arrange(inner);
        }

        if (on_select_) {
            on_select_(index);
        }
        invalidate();
    }
}

D2DContainerComponent* D2DTabControl::selectedContent() const noexcept {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        return tabs_[selected_index_].content.get();
    }
    return nullptr;
}

void D2DTabControl::createResources(DeviceResources& resources) {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return;
    }

    // Create text format
    if (!text_format_) {
        factory.dwriteFactory()->CreateTextFormat(
            Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);

        if (text_format_) {
            text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    // Create brushes
    tab_bg_brush_ = resources.createSolidBrush(Style::tabBackground());
    tab_bg_active_brush_ = resources.createSolidBrush(Style::tabBackgroundActive());
    tab_bg_hover_brush_ = resources.createSolidBrush(Style::tabBackgroundHover());
    tab_border_brush_ = resources.createSolidBrush(Style::tabBorder());
    tab_text_brush_ = resources.createSolidBrush(Style::tabText());
    content_bg_brush_ = resources.createSolidBrush(Style::contentBackground());
    content_border_brush_ = resources.createSolidBrush(Style::contentBorder());

    updateTabWidths();
}

void D2DTabControl::updateTabWidths() {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid() || !text_format_) {
        return;
    }

    for (auto& tab : tabs_) {
        if (tab.title.empty()) {
            tab.title_width = 0.0f;
            continue;
        }

        ComPtr<IDWriteTextLayout> layout;
        factory.dwriteFactory()->CreateTextLayout(
            tab.title.c_str(), static_cast<UINT32>(tab.title.length()), text_format_.Get(),
            10000.0f, Style::tabHeight(), &layout);

        if (layout) {
            DWRITE_TEXT_METRICS metrics;
            layout->GetMetrics(&metrics);
            tab.title_width = metrics.widthIncludingTrailingWhitespace;
        }
    }
}

void D2DTabControl::layoutTabs() {
    // Use local coordinates (0,0 = top-left of this component)
    float x = 0.0f;
    float y = 0.0f;
    float tab_height = Style::tabHeight();
    float padding = Style::tabPadding();

    for (auto& tab : tabs_) {
        float tab_width = tab.title_width + padding * 2;
        tab.tab_bounds = Rect{x, y, tab_width, tab_height};
        x += tab_width;
    }

    // Tab strip bounds (local coordinates)
    tab_strip_bounds_ = Rect{0.0f, 0.0f, bounds_.width, tab_height};

    // Content bounds (local coordinates)
    content_bounds_ = Rect{0.0f, tab_height, bounds_.width, bounds_.height - tab_height};

    // Arrange selected content (using absolute coordinates for the content's bounds)
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            // Convert local content_bounds_ to absolute for the content's arrangement
            Rect abs_content = content_bounds_.translated(bounds_.x, bounds_.y);
            Rect inner = abs_content.deflated(Style::borderWidth(), Style::borderWidth());
            content->arrange(inner);
        }
    }
}

Size D2DTabControl::measure(const Size& available_size) {
    float tab_height = Style::tabHeight();

    // Measure tab strip width
    float tabs_width = 0.0f;
    float padding = Style::tabPadding();
    for (const auto& tab : tabs_) {
        tabs_width += tab.title_width + padding * 2;
    }

    // Measure content
    Size content_size;
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            Size content_available{available_size.width, available_size.height - tab_height};
            content_size = content->measure(content_available);
        }
    }

    float width = std::max(tabs_width, content_size.width);
    float height = tab_height + content_size.height;

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DTabControl::arrange(const Rect& bounds) {
    D2DContainerComponent::arrange(bounds);
    layoutTabs();
}

void D2DTabControl::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    // Convert local bounds to absolute for rendering
    Rect abs_content = content_bounds_.translated(bounds_.x, bounds_.y);

    // Draw content area background and border
    if (content_bg_brush_) {
        rt->FillRectangle(abs_content.toD2D(), content_bg_brush_.Get());
    }
    if (content_border_brush_) {
        rt->DrawRectangle(abs_content.toD2D(), content_border_brush_.Get(), Style::borderWidth());
    }

    // Draw tabs
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        const auto& tab = tabs_[i];
        bool is_selected = (i == selected_index_);
        bool is_hovered = (i == hovered_index_);

        // Convert local tab bounds to absolute
        Rect abs_tab = tab.tab_bounds.translated(bounds_.x, bounds_.y);

        // Determine background brush
        ID2D1SolidColorBrush* bg_brush = tab_bg_brush_.Get();
        if (is_selected) {
            bg_brush = tab_bg_active_brush_.Get();
        } else if (is_hovered) {
            bg_brush = tab_bg_hover_brush_.Get();
        }

        // Draw tab background
        D2D1_RECT_F tab_rect = abs_tab.toD2D();

        // For selected tab, extend it down to cover the content border
        if (is_selected) {
            tab_rect.bottom += Style::borderWidth();
        }

        if (bg_brush) {
            rt->FillRectangle(tab_rect, bg_brush);
        }

        // Draw tab border (top and sides only for selected)
        if (tab_border_brush_) {
            D2D1_POINT_2F p1, p2, p3, p4;
            p1 = D2D1::Point2F(abs_tab.x, abs_tab.bottom());
            p2 = D2D1::Point2F(abs_tab.x, abs_tab.y);
            p3 = D2D1::Point2F(abs_tab.right(), abs_tab.y);
            p4 = D2D1::Point2F(abs_tab.right(), abs_tab.bottom());

            rt->DrawLine(p1, p2, tab_border_brush_.Get(), Style::borderWidth());  // Left
            rt->DrawLine(p2, p3, tab_border_brush_.Get(), Style::borderWidth());  // Top
            rt->DrawLine(p3, p4, tab_border_brush_.Get(), Style::borderWidth());  // Right

            // Bottom line only for non-selected tabs
            if (!is_selected) {
                rt->DrawLine(p4, p1, tab_border_brush_.Get(), Style::borderWidth());
            }
        }

        // Draw tab text
        if (tab_text_brush_ && text_format_) {
            rt->DrawTextW(tab.title.c_str(), static_cast<UINT32>(tab.title.length()),
                          text_format_.Get(), abs_tab.toD2D(), tab_text_brush_.Get());
        }
    }

    // Draw bottom border for unused tab strip area
    if (tab_border_brush_) {
        float last_tab_right = tabs_.empty() ? 0.0f : tabs_.back().tab_bounds.right();
        float abs_last_right = bounds_.x + last_tab_right;
        float abs_content_y = bounds_.y + content_bounds_.y;
        if (abs_last_right < bounds_.right()) {
            D2D1_POINT_2F p1 = D2D1::Point2F(abs_last_right, abs_content_y);
            D2D1_POINT_2F p2 = D2D1::Point2F(bounds_.right(), abs_content_y);
            rt->DrawLine(p1, p2, tab_border_brush_.Get(), Style::borderWidth());
        }
    }

    // Render selected content
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            // Clip to content area (absolute coordinates)
            rt->PushAxisAlignedClip(
                abs_content.deflated(Style::borderWidth(), Style::borderWidth()).toD2D(),
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            content->render(rt);
            rt->PopAxisAlignedClip();
        }
    }
}

int D2DTabControl::hitTestTab(const Point& point) const {
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        if (tabs_[i].tab_bounds.contains(point)) {
            return i;
        }
    }
    return -1;
}

bool D2DTabControl::onMouseMove(const MouseEvent& event) {
    // event.position is in TabControl's local coordinates
    int new_hover = hitTestTab(event.position);
    if (new_hover != hovered_index_) {
        hovered_index_ = new_hover;
        invalidate();
    }

    // Forward to content if in content area (local coordinates)
    if (content_bounds_.contains(event.position)) {
        if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
            auto& content = tabs_[selected_index_].content;
            if (content) {
                // Transform to content's local coordinates
                // content_bounds_ is local, so subtract its position
                MouseEvent content_event = event;
                content_event.position.x -= content_bounds_.x;
                content_event.position.y -= content_bounds_.y;
                return content->onMouseMove(content_event);
            }
        }
    }

    return false;
}

bool D2DTabControl::onMouseDown(const MouseEvent& event) {
    if (event.button != MouseButton::Left) {
        return false;
    }

    // event.position is in TabControl's local coordinates
    // Check tab click (tab_bounds are in local coordinates)
    int tab = hitTestTab(event.position);
    if (tab >= 0) {
        setSelectedIndex(tab);
        return true;
    }

    // Forward to content if in content area (local coordinates)
    if (content_bounds_.contains(event.position)) {
        if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
            auto& content = tabs_[selected_index_].content;
            if (content) {
                // Transform to content's local coordinates
                MouseEvent content_event = event;
                content_event.position.x -= content_bounds_.x;
                content_event.position.y -= content_bounds_.y;
                return content->onMouseDown(content_event);
            }
        }
    }

    return false;
}

bool D2DTabControl::onMouseUp(const MouseEvent& event) {
    // Forward to content if in content area (local coordinates)
    if (content_bounds_.contains(event.position)) {
        if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
            auto& content = tabs_[selected_index_].content;
            if (content) {
                // Transform to content's local coordinates
                MouseEvent content_event = event;
                content_event.position.x -= content_bounds_.x;
                content_event.position.y -= content_bounds_.y;
                return content->onMouseUp(content_event);
            }
        }
    }

    return false;
}

bool D2DTabControl::onMouseLeave(const MouseEvent& event) {
    if (hovered_index_ >= 0) {
        hovered_index_ = -1;
        invalidate();
    }
    return false;
}

bool D2DTabControl::onKeyDown(const KeyEvent& event) {
    // Forward to selected tab's content
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            return content->onKeyDown(event);
        }
    }
    return false;
}

bool D2DTabControl::onKeyUp(const KeyEvent& event) {
    // Forward to selected tab's content
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            return content->onKeyUp(event);
        }
    }
    return false;
}

bool D2DTabControl::onChar(const KeyEvent& event) {
    // Forward to selected tab's content
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(tabs_.size())) {
        auto& content = tabs_[selected_index_].content;
        if (content) {
            return content->onChar(event);
        }
    }
    return false;
}

}  // namespace nive::ui::d2d
