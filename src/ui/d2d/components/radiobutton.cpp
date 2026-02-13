/// @file radiobutton.cpp
/// @brief Implementation of D2D radio button component

#include "radiobutton.hpp"

#include <algorithm>

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

// D2DRadioButton implementation

D2DRadioButton::D2DRadioButton() = default;

D2DRadioButton::D2DRadioButton(const std::wstring& text) : text_(text) {
}

void D2DRadioButton::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        invalidate();
    }
}

void D2DRadioButton::setSelected(bool selected) {
    if (selected_ != selected) {
        selected_ = selected;
        // Update group's tracking of selected button
        if (group_) {
            group_->updateSelectionTracking(this, selected);
        }
        if (on_change_) {
            on_change_(selected_);
        }
        invalidate();
    }
}

void D2DRadioButton::setGroup(D2DRadioGroup* group) {
    if (group_ != group) {
        if (group_) {
            group_->removeButton(this);
        }
        group_ = group;
        if (group_) {
            group_->addButton(this);
        }
    }
}

void D2DRadioButton::select() {
    if (group_) {
        group_->onButtonSelected(this);
    } else {
        setSelected(true);
    }
}

Rect D2DRadioButton::getCircleRect() const noexcept {
    float circle_size = Style::circleSize();
    float circle_y = bounds_.y + (bounds_.height - circle_size) / 2.0f;
    return Rect{bounds_.x, circle_y, circle_size, circle_size};
}

void D2DRadioButton::createResources(DeviceResources& resources) {
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
    Color circle_bg, circle_border, text_color;

    if (!enabled_) {
        circle_bg = Style::disabledBackground();
        circle_border = Style::disabledBorder();
        text_color = Style::disabledTextColor();
    } else {
        circle_bg = Style::circleBackground();
        circle_border = hovered_ ? Style::circleHoverBorder() : Style::circleBorder();
        text_color = Style::textColor();
    }

    circle_bg_brush_ = resources.createSolidBrush(circle_bg);
    circle_border_brush_ = resources.createSolidBrush(circle_border);
    selected_fill_brush_ = resources.createSolidBrush(Style::selectedFill());
    text_brush_ = resources.createSolidBrush(text_color);
}

Size D2DRadioButton::measure(const Size& available_size) {
    float circle_size = Style::circleSize();
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

    float width = circle_size + spacing + text_width;
    float height = std::max(circle_size, text_height);

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DRadioButton::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    Rect circle_rect = getCircleRect();
    float cx = circle_rect.x + circle_rect.width / 2.0f;
    float cy = circle_rect.y + circle_rect.height / 2.0f;
    float radius = circle_rect.width / 2.0f;

    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);

    // Draw outer circle
    if (circle_bg_brush_) {
        rt->FillEllipse(ellipse, circle_bg_brush_.Get());
    }
    if (circle_border_brush_) {
        rt->DrawEllipse(ellipse, circle_border_brush_.Get(), Style::borderWidth());
    }

    // Draw inner circle if selected
    if (selected_ && selected_fill_brush_) {
        float inner_radius = Style::innerCircleSize() / 2.0f;
        D2D1_ELLIPSE inner_ellipse =
            D2D1::Ellipse(D2D1::Point2F(cx, cy), inner_radius, inner_radius);
        rt->FillEllipse(inner_ellipse, selected_fill_brush_.Get());
    }

    // Draw text
    if (!text_.empty() && text_format_ && text_brush_) {
        float text_x = circle_rect.right() + Style::spacing();
        D2D1_RECT_F text_rect = D2D1::RectF(text_x, bounds_.y, bounds_.right(), bounds_.bottom());

        rt->DrawTextW(text_.c_str(), static_cast<UINT32>(text_.length()), text_format_.Get(),
                      text_rect, text_brush_.Get());
    }
}

bool D2DRadioButton::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }
    pressed_ = true;
    return true;
}

bool D2DRadioButton::onMouseUp(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    // event.position is in local coordinates (0,0 = top-left of this component)
    bool in_bounds = event.position.x >= 0 && event.position.x < bounds_.width &&
                     event.position.y >= 0 && event.position.y < bounds_.height;
    if (pressed_ && in_bounds) {
        select();
    }
    pressed_ = false;
    return true;
}

bool D2DRadioButton::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.keyCode == VK_SPACE) {
        select();
        return true;
    }

    return false;
}

// D2DRadioGroup implementation

void D2DRadioGroup::addButton(D2DRadioButton* button) {
    if (std::find(buttons_.begin(), buttons_.end(), button) == buttons_.end()) {
        buttons_.push_back(button);
    }
}

void D2DRadioGroup::removeButton(D2DRadioButton* button) {
    auto it = std::find(buttons_.begin(), buttons_.end(), button);
    if (it != buttons_.end()) {
        buttons_.erase(it);
        if (selected_ == button) {
            selected_ = nullptr;
        }
    }
}

void D2DRadioGroup::onButtonSelected(D2DRadioButton* button) {
    if (selected_ == button) {
        return;
    }

    // Deselect previous
    if (selected_) {
        selected_->setSelected(false);
    }

    // Select new
    selected_ = button;
    if (selected_) {
        selected_->setSelected(true);
    }
}

void D2DRadioGroup::updateSelectionTracking(D2DRadioButton* button, bool selected) {
    if (selected) {
        // Button is being selected
        if (selected_ == button) {
            return;  // Already tracked
        }
        D2DRadioButton* old = selected_;
        selected_ = button;  // Update tracking first to prevent recursion issues
        // Deselect old button (will call back with selected=false, which is handled below)
        if (old) {
            old->setSelected(false);
        }
    } else {
        // Button is being deselected - only clear tracking if it's the currently tracked one
        if (selected_ == button) {
            selected_ = nullptr;
        }
    }
}

}  // namespace nive::ui::d2d
