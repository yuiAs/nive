/// @file groupbox.cpp
/// @brief Implementation of D2D group box component

#include "groupbox.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DGroupBox::D2DGroupBox() = default;

D2DGroupBox::D2DGroupBox(const std::wstring& title) : title_(title) {
}

void D2DGroupBox::setTitle(const std::wstring& title) {
    if (title_ != title) {
        title_ = title;
        text_layout_.Reset();
        invalidate();
    }
}

Rect D2DGroupBox::contentArea() const noexcept {
    Thickness pad = Style::padding();
    float top_offset = title_height_ > 0 ? title_height_ / 2.0f + pad.top : pad.top;

    return Rect{bounds_.x + pad.left, bounds_.y + top_offset, bounds_.width - pad.left - pad.right,
                bounds_.height - top_offset - pad.bottom};
}

void D2DGroupBox::createResources(DeviceResources& resources) {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return;
    }

    // Create text format
    if (!text_format_) {
        factory.dwriteFactory()->CreateTextFormat(
            Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);
    }

    // Measure title
    if (!title_.empty() && text_format_) {
        text_layout_.Reset();
        factory.dwriteFactory()->CreateTextLayout(
            title_.c_str(), static_cast<UINT32>(title_.length()), text_format_.Get(), 10000.0f,
            10000.0f, &text_layout_);

        if (text_layout_) {
            DWRITE_TEXT_METRICS metrics;
            text_layout_->GetMetrics(&metrics);
            title_width_ = metrics.widthIncludingTrailingWhitespace;
            title_height_ = metrics.height;
        }
    } else {
        title_width_ = 0.0f;
        title_height_ = 0.0f;
    }

    // Create brushes
    Color text_color = enabled_ ? Style::textColor() : Style::disabledTextColor();
    border_brush_ = resources.createSolidBrush(Style::border());
    text_brush_ = resources.createSolidBrush(text_color);
    label_bg_brush_ = resources.createSolidBrush(Style::labelBackground());
}

Size D2DGroupBox::measure(const Size& available_size) {
    Thickness pad = Style::padding();
    float title_offset = title_height_ > 0 ? title_height_ / 2.0f : 0.0f;

    // Calculate content area size
    Size content_available{available_size.width - pad.left - pad.right,
                           available_size.height - title_offset - pad.top - pad.bottom};

    // Measure children
    float max_child_right = 0.0f;
    float max_child_bottom = 0.0f;

    for (auto& child : children_) {
        Size child_size = child->measure(content_available);
        // Track maximum extents (children are positioned manually)
        max_child_right = std::max(max_child_right, child_size.width);
        max_child_bottom = std::max(max_child_bottom, child_size.height);
    }

    // Calculate total size
    float width = std::max(title_width_ + pad.left * 2, max_child_right + pad.left + pad.right);
    float height = title_offset + pad.top + max_child_bottom + pad.bottom;

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DGroupBox::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    float border_radius = Style::borderRadius();
    float border_width = Style::borderWidth();
    float title_offset = title_height_ / 2.0f;
    float title_padding = 4.0f;

    // The border starts below the title text center
    D2D1_RECT_F border_rect =
        D2D1::RectF(bounds_.x + border_width / 2.0f, bounds_.y + title_offset + border_width / 2.0f,
                    bounds_.right() - border_width / 2.0f, bounds_.bottom() - border_width / 2.0f);

    // Create a geometry for the border with a gap for the title
    if (border_brush_) {
        auto& factory = D2DFactory::instance();
        if (factory.isValid()) {
            // For simplicity, draw as four separate lines/arcs
            // A more sophisticated approach would use path geometry

            // Draw rounded rectangle border
            D2D1_ROUNDED_RECT rounded_rect =
                D2D1::RoundedRect(border_rect, border_radius, border_radius);

            // Save the transform
            D2D1_MATRIX_3X2_F original_transform;
            rt->GetTransform(&original_transform);

            // Create clip rect that excludes title area
            if (!title_.empty()) {
                // We'll draw the border first, then overdraw with background
                // to create the gap effect
                rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), border_width);

                // Create a small filled rectangle to "erase" the border where title goes
                float title_x = bounds_.x + Style::padding().left;
                D2D1_RECT_F title_gap_rect =
                    D2D1::RectF(title_x - title_padding, bounds_.y,
                                title_x + title_width_ + title_padding, bounds_.y + title_height_);

                // Use style-defined background color
                if (label_bg_brush_) {
                    rt->FillRectangle(title_gap_rect, label_bg_brush_.Get());
                }
            } else {
                rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), border_width);
            }

            rt->SetTransform(original_transform);
        }
    }

    // Draw title text
    if (!title_.empty() && text_format_ && text_brush_) {
        float title_x = bounds_.x + Style::padding().left;
        float title_y = bounds_.y;

        D2D1_RECT_F text_rect =
            D2D1::RectF(title_x, title_y, title_x + title_width_, title_y + title_height_);

        rt->DrawTextW(title_.c_str(), static_cast<UINT32>(title_.length()), text_format_.Get(),
                      text_rect, text_brush_.Get());
    }

    // Render children
    renderChildren(rt);
}

}  // namespace nive::ui::d2d
