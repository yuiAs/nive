/// @file spinedit.cpp
/// @brief Implementation of D2D spin edit component

#include "spinedit.hpp"

#include <charconv>
#include <format>

#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

D2DSpinEdit::D2DSpinEdit() {
    edit_ = std::make_unique<D2DEditBox>();
    edit_->setInputMode(InputMode::Integer);
    edit_->onChange([this](const std::wstring&) { parseEditText(); });
    updateEditText();
}

void D2DSpinEdit::setValue(int value) {
    int old_value = value_;
    value_ = value;
    clampValue();

    if (value_ != old_value) {
        updateEditText();
        if (on_change_) {
            on_change_(value_);
        }
        invalidate();
    }
}

void D2DSpinEdit::setMinimum(int min) {
    min_value_ = min;
    clampValue();
    updateEditText();
    invalidate();
}

void D2DSpinEdit::setMaximum(int max) {
    max_value_ = max;
    clampValue();
    updateEditText();
    invalidate();
}

void D2DSpinEdit::setRange(int min, int max) {
    min_value_ = min;
    max_value_ = max;
    clampValue();
    updateEditText();
    invalidate();
}

void D2DSpinEdit::setStep(int step) {
    step_ = step > 0 ? step : 1;
}

void D2DSpinEdit::increment() {
    setValue(value_ + step_);
}

void D2DSpinEdit::decrement() {
    setValue(value_ - step_);
}

void D2DSpinEdit::updateEditText() {
    edit_->setText(std::to_wstring(value_));
}

void D2DSpinEdit::parseEditText() {
    const std::wstring& text = edit_->text();
    if (text.empty()) {
        return;
    }

    // Convert wide string to narrow for std::from_chars
    std::string narrow;
    for (wchar_t ch : text) {
        if (ch >= L'0' && ch <= L'9') {
            narrow += static_cast<char>(ch);
        } else if (ch == L'-' && narrow.empty()) {
            narrow += '-';
        }
    }

    if (narrow.empty() || (narrow == "-")) {
        return;
    }

    int result = 0;
    auto [ptr, ec] = std::from_chars(narrow.data(), narrow.data() + narrow.size(), result);
    if (ec == std::errc()) {
        int old_value = value_;
        value_ = result;
        clampValue();
        if (value_ != old_value && on_change_) {
            on_change_(value_);
        }
    }
}

void D2DSpinEdit::clampValue() {
    if (value_ < min_value_) {
        value_ = min_value_;
    } else if (value_ > max_value_) {
        value_ = max_value_;
    }
}

void D2DSpinEdit::createResources(DeviceResources& resources) {
    edit_->createResources(resources);

    // Create button brushes
    using ButtonStyle = StyleTraits<D2DButton>;
    button_bg_brush_ = resources.createSolidBrush(ButtonStyle::background());
    button_hover_brush_ = resources.createSolidBrush(ButtonStyle::hoverBackground());
    button_pressed_brush_ = resources.createSolidBrush(ButtonStyle::pressedBackground());
    button_border_brush_ = resources.createSolidBrush(ButtonStyle::border());
    arrow_brush_ = resources.createSolidBrush(CommonStyle::foreground());
}

Size D2DSpinEdit::measure(const Size& available_size) {
    // Same height as edit box
    float height = StyleTraits<D2DEditBox>::height();
    float width = available_size.width > 0 ? available_size.width : 100.0f;

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DSpinEdit::arrange(const Rect& bounds) {
    D2DUIComponent::arrange(bounds);

    // Edit box takes most of the space
    float edit_width = bounds.width - kButtonWidth;
    edit_->arrange(Rect{bounds.x, bounds.y, edit_width, bounds.height});

    // Up and down buttons on the right
    float button_height = bounds.height / 2.0f;
    float button_x = bounds.x + edit_width;

    up_button_bounds_ = Rect{button_x, bounds.y, kButtonWidth, button_height};
    down_button_bounds_ = Rect{button_x, bounds.y + button_height, kButtonWidth, button_height};
}

void D2DSpinEdit::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    // Render edit box
    edit_->render(rt);

    // Render up button
    {
        ID2D1SolidColorBrush* bg = button_bg_brush_.Get();
        if (up_pressed_) {
            bg = button_pressed_brush_.Get();
        } else if (up_hovered_) {
            bg = button_hover_brush_.Get();
        }

        D2D1_RECT_F rect = up_button_bounds_.toD2D();
        if (bg) {
            rt->FillRectangle(rect, bg);
        }
        if (button_border_brush_) {
            rt->DrawRectangle(rect, button_border_brush_.Get(), 1.0f);
        }

        // Draw up arrow
        if (arrow_brush_) {
            float cx = up_button_bounds_.x + up_button_bounds_.width / 2.0f;
            float cy = up_button_bounds_.y + up_button_bounds_.height / 2.0f;
            float size = 4.0f;

            D2D1_POINT_2F p1 = D2D1::Point2F(cx, cy - size);
            D2D1_POINT_2F p2 = D2D1::Point2F(cx - size, cy + size / 2.0f);
            D2D1_POINT_2F p3 = D2D1::Point2F(cx + size, cy + size / 2.0f);

            rt->DrawLine(p1, p2, arrow_brush_.Get(), 1.5f);
            rt->DrawLine(p1, p3, arrow_brush_.Get(), 1.5f);
        }
    }

    // Render down button
    {
        ID2D1SolidColorBrush* bg = button_bg_brush_.Get();
        if (down_pressed_) {
            bg = button_pressed_brush_.Get();
        } else if (down_hovered_) {
            bg = button_hover_brush_.Get();
        }

        D2D1_RECT_F rect = down_button_bounds_.toD2D();
        if (bg) {
            rt->FillRectangle(rect, bg);
        }
        if (button_border_brush_) {
            rt->DrawRectangle(rect, button_border_brush_.Get(), 1.0f);
        }

        // Draw down arrow
        if (arrow_brush_) {
            float cx = down_button_bounds_.x + down_button_bounds_.width / 2.0f;
            float cy = down_button_bounds_.y + down_button_bounds_.height / 2.0f;
            float size = 4.0f;

            D2D1_POINT_2F p1 = D2D1::Point2F(cx, cy + size);
            D2D1_POINT_2F p2 = D2D1::Point2F(cx - size, cy - size / 2.0f);
            D2D1_POINT_2F p3 = D2D1::Point2F(cx + size, cy - size / 2.0f);

            rt->DrawLine(p1, p2, arrow_brush_.Get(), 1.5f);
            rt->DrawLine(p1, p3, arrow_brush_.Get(), 1.5f);
        }
    }
}

bool D2DSpinEdit::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    // Request focus for this SpinEdit (will propagate to internal EditBox via onFocusChanged)
    if (parent_) {
        parent_->requestFocus(this);
    }

    // event.position is in local coordinates, convert to absolute for hit test
    // since button bounds are stored in absolute coordinates
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    if (up_button_bounds_.contains(abs_pos)) {
        up_pressed_ = true;
        increment();
        invalidate();
        return true;
    }

    if (down_button_bounds_.contains(abs_pos)) {
        down_pressed_ = true;
        decrement();
        invalidate();
        return true;
    }

    // Forward to edit box - transform to edit's local coordinates
    MouseEvent edit_event = event;
    edit_event.position.x -= (edit_->bounds().x - bounds_.x);
    edit_event.position.y -= (edit_->bounds().y - bounds_.y);
    return edit_->onMouseDown(edit_event);
}

bool D2DSpinEdit::onMouseUp(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    up_pressed_ = false;
    down_pressed_ = false;
    invalidate();

    // Forward to edit box - transform to edit's local coordinates
    MouseEvent edit_event = event;
    edit_event.position.x -= (edit_->bounds().x - bounds_.x);
    edit_event.position.y -= (edit_->bounds().y - bounds_.y);
    return edit_->onMouseUp(edit_event);
}

bool D2DSpinEdit::onMouseWheel(const MouseEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.wheelDelta > 0) {
        increment();
    } else if (event.wheelDelta < 0) {
        decrement();
    }
    return true;
}

bool D2DSpinEdit::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.keyCode == VK_UP) {
        increment();
        return true;
    }
    if (event.keyCode == VK_DOWN) {
        decrement();
        return true;
    }

    return edit_->onKeyDown(event);
}

bool D2DSpinEdit::onChar(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    // Input filtering is handled by EditBox's InputMode
    return edit_->onChar(event);
}

void D2DSpinEdit::onFocusChanged(const FocusEvent& event) {
    // Properly set focus state on internal EditBox
    edit_->setFocused(event.gained);
    if (!event.gained) {
        // Parse and validate on focus loss
        parseEditText();
        updateEditText();  // Normalize display
    }
}

}  // namespace nive::ui::d2d
