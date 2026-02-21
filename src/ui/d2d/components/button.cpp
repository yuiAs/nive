/// @file button.cpp
/// @brief Implementation of D2D button component

#include "button.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DButton::D2DButton() = default;

D2DButton::D2DButton(const std::wstring& text) : text_(text) {
}

void D2DButton::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        text_layout_.Reset();
        invalidate();
    }
}

void D2DButton::setVariant(ButtonVariant variant) {
    if (variant_ != variant) {
        variant_ = variant;
        resources_dirty_ = true;
        invalidate();
    }
}

ButtonState D2DButton::currentState() const noexcept {
    if (!enabled_) {
        return ButtonState::Disabled;
    }
    if (pressed_) {
        return ButtonState::Pressed;
    }
    if (hovered_) {
        return ButtonState::Hover;
    }
    return ButtonState::Normal;
}

void D2DButton::createResources(DeviceResources& resources) {
    if (resources_dirty_ || !text_format_) {
        auto& factory = D2DFactory::instance();
        if (factory.isValid()) {
            text_format_.Reset();
            factory.dwriteFactory()->CreateTextFormat(
                Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);

            if (text_format_) {
                text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        }
        resources_dirty_ = false;
    }

    updateBrushes(resources);
}

void D2DButton::updateBrushes(DeviceResources& resources) {
    ButtonState state = currentState();

    Color bg_color, border_color, text_color;

    if (variant_ == ButtonVariant::Primary) {
        switch (state) {
        case ButtonState::Disabled:
            bg_color = Style::disabledBackground();
            border_color = Style::disabledBorder();
            text_color = Style::disabledTextColor();
            break;
        case ButtonState::Pressed:
            bg_color = Style::primaryPressedBackground();
            border_color = Style::primaryPressedBackground();
            text_color = Style::primaryTextColor();
            break;
        case ButtonState::Hover:
            bg_color = Style::primaryHoverBackground();
            border_color = Style::primaryHoverBackground();
            text_color = Style::primaryTextColor();
            break;
        default:
            bg_color = Style::primaryBackground();
            border_color = Style::primaryBackground();
            text_color = Style::primaryTextColor();
            break;
        }
    } else {
        switch (state) {
        case ButtonState::Disabled:
            bg_color = Style::disabledBackground();
            border_color = Style::disabledBorder();
            text_color = Style::disabledTextColor();
            break;
        case ButtonState::Pressed:
            bg_color = Style::pressedBackground();
            border_color = Style::pressedBorder();
            text_color = Style::textColor();
            break;
        case ButtonState::Hover:
            bg_color = Style::hoverBackground();
            border_color = Style::hoverBorder();
            text_color = Style::textColor();
            break;
        default:
            bg_color = Style::background();
            border_color = focused_ ? Style::focusedBorder() : Style::border();
            text_color = Style::textColor();
            break;
        }
    }

    background_brush_ = resources.createSolidBrush(bg_color);
    border_brush_ = resources.createSolidBrush(border_color);
    text_brush_ = resources.createSolidBrush(text_color);
}

Size D2DButton::measure(const Size& available_size) {
    // Get text size
    auto& factory = D2DFactory::instance();
    float text_width = 0.0f;
    float text_height = Style::fontSize();

    if (factory.isValid() && text_format_ && !text_.empty()) {
        ComPtr<IDWriteTextLayout> layout;
        factory.dwriteFactory()->CreateTextLayout(text_.c_str(),
                                                  static_cast<UINT32>(text_.length()),
                                                  text_format_.Get(), 10000.0f, 10000.0f, &layout);

        if (layout) {
            DWRITE_TEXT_METRICS metrics;
            layout->GetMetrics(&metrics);
            text_width = metrics.widthIncludingTrailingWhitespace;
            text_height = metrics.height;
        }
    }

    // Add padding
    Thickness pad = Style::padding();
    float width = text_width + pad.horizontalTotal();
    float height = text_height + pad.verticalTotal();

    // Apply minimum size
    width = std::max(width, Style::minWidth());
    height = std::max(height, Style::height());

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DButton::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    // Note: Do NOT use dynamic_cast on COM interfaces - they don't support C++ RTTI

    if (!background_brush_ || !border_brush_ || !text_brush_) {
        return;
    }

    D2D1_RECT_F rect = bounds_.toD2D();
    D2D1_ROUNDED_RECT rounded_rect =
        D2D1::RoundedRect(rect, Style::borderRadius(), Style::borderRadius());

    // Draw background
    rt->FillRoundedRectangle(rounded_rect, background_brush_.Get());

    // Draw border
    rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), Style::borderWidth());

    // Draw text
    if (!text_.empty() && text_format_) {
        rt->DrawTextW(text_.c_str(), static_cast<UINT32>(text_.length()), text_format_.Get(), rect,
                      text_brush_.Get());
    }
}

bool D2DButton::onMouseEnter(const MouseEvent& event) {
    invalidate();
    return true;
}

bool D2DButton::onMouseLeave(const MouseEvent& event) {
    pressed_ = false;
    invalidate();
    return true;
}

bool D2DButton::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }
    if (parent_) {
        parent_->requestFocus(this);
    }
    pressed_ = true;
    invalidate();
    return true;
}

bool D2DButton::onMouseUp(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    bool was_pressed = pressed_;
    pressed_ = false;
    invalidate();

    // Fire click if we were pressed and mouse is still over button
    // Note: event.position is in local coordinates (0,0 is top-left of this component)
    bool in_bounds = event.position.x >= 0 && event.position.x < bounds_.width &&
                     event.position.y >= 0 && event.position.y < bounds_.height;
    if (was_pressed && in_bounds && on_click_) {
        on_click_();
    }

    return true;
}

bool D2DButton::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    // Space or Enter activates the button
    if (event.keyCode == VK_SPACE || event.keyCode == VK_RETURN) {
        if (on_click_) {
            on_click_();
        }
        return true;
    }

    return false;
}

}  // namespace nive::ui::d2d
