/// @file checkbox.cpp
/// @brief Implementation of D2D checkbox component

#include "checkbox.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DCheckBox::D2DCheckBox() = default;

D2DCheckBox::D2DCheckBox(const std::wstring& text) : text_(text) {
}

void D2DCheckBox::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        invalidate();
    }
}

void D2DCheckBox::setChecked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        invalidate();
    }
}

void D2DCheckBox::toggle() {
    checked_ = !checked_;
    if (on_change_) {
        on_change_(checked_);
    }
    invalidate();
}

Rect D2DCheckBox::getBoxRect() const noexcept {
    float box_size = Style::boxSize();
    float box_y = bounds_.y + (bounds_.height - box_size) / 2.0f;
    return Rect{bounds_.x, box_y, box_size, box_size};
}

void D2DCheckBox::createResources(DeviceResources& resources) {
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
            text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    // Create brushes based on state
    Color box_bg, box_border, text_color;

    if (!enabled_) {
        box_bg = Style::disabledBackground();
        box_border = Style::disabledBorder();
        text_color = Style::disabledTextColor();
    } else if (checked_) {
        box_bg = Style::checkedBackground();
        box_border = Style::checkedBorder();
        text_color = Style::textColor();
    } else {
        box_bg = Style::boxBackground();
        box_border = hovered_ ? Style::boxHoverBorder() : Style::boxBorder();
        text_color = Style::textColor();
    }

    box_bg_brush_ = resources.createSolidBrush(box_bg);
    box_border_brush_ = resources.createSolidBrush(box_border);
    checkmark_brush_ = resources.createSolidBrush(Style::checkmarkColor());
    text_brush_ = resources.createSolidBrush(text_color);
}

Size D2DCheckBox::measure(const Size& available_size) {
    float box_size = Style::boxSize();
    float spacing = Style::spacing();

    // Measure text width
    float text_width = 0.0f;
    float text_height = Style::fontSize();

    if (!text_.empty() && text_format_) {
        auto& factory = D2DFactory::instance();
        if (factory.isValid()) {
            ComPtr<IDWriteTextLayout> layout;
            factory.dwriteFactory()->CreateTextLayout(
                text_.c_str(), static_cast<UINT32>(text_.length()), text_format_.Get(), 10000.0f,
                10000.0f, &layout);

            if (layout) {
                DWRITE_TEXT_METRICS metrics;
                layout->GetMetrics(&metrics);
                text_width = metrics.widthIncludingTrailingWhitespace;
                text_height = metrics.height;
            }
        }
    }

    float width = box_size + spacing + text_width;
    float height = std::max(box_size, text_height);

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DCheckBox::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    // Draw checkbox box
    Rect box_rect = getBoxRect();
    D2D1_RECT_F d2d_box = box_rect.toD2D();
    D2D1_ROUNDED_RECT rounded_box =
        D2D1::RoundedRect(d2d_box, Style::borderRadius(), Style::borderRadius());

    if (box_bg_brush_) {
        rt->FillRoundedRectangle(rounded_box, box_bg_brush_.Get());
    }
    if (box_border_brush_) {
        rt->DrawRoundedRectangle(rounded_box, box_border_brush_.Get(), Style::borderWidth());
    }

    // Draw checkmark if checked
    if (checked_ && checkmark_brush_) {
        // Draw a simple checkmark using lines
        float cx = box_rect.x + box_rect.width / 2.0f;
        float cy = box_rect.y + box_rect.height / 2.0f;
        float scale = box_rect.width / 16.0f;

        D2D1_POINT_2F p1 = D2D1::Point2F(cx - 4.0f * scale, cy);
        D2D1_POINT_2F p2 = D2D1::Point2F(cx - 1.0f * scale, cy + 3.0f * scale);
        D2D1_POINT_2F p3 = D2D1::Point2F(cx + 4.0f * scale, cy - 3.0f * scale);

        rt->DrawLine(p1, p2, checkmark_brush_.Get(), 2.0f * scale);
        rt->DrawLine(p2, p3, checkmark_brush_.Get(), 2.0f * scale);
    }

    // Draw text
    if (!text_.empty() && text_format_ && text_brush_) {
        float text_x = box_rect.right() + Style::spacing();
        D2D1_RECT_F text_rect = D2D1::RectF(text_x, bounds_.y, bounds_.right(), bounds_.bottom());

        rt->DrawTextW(text_.c_str(), static_cast<UINT32>(text_.length()), text_format_.Get(),
                      text_rect, text_brush_.Get());
    }
}

bool D2DCheckBox::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }
    pressed_ = true;
    return true;
}

bool D2DCheckBox::onMouseUp(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    // event.position is in local coordinates (0,0 = top-left of this component)
    bool in_bounds = event.position.x >= 0 && event.position.x < bounds_.width &&
                     event.position.y >= 0 && event.position.y < bounds_.height;
    if (pressed_ && in_bounds) {
        toggle();
    }
    pressed_ = false;
    return true;
}

bool D2DCheckBox::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.keyCode == VK_SPACE) {
        toggle();
        return true;
    }

    return false;
}

}  // namespace nive::ui::d2d
