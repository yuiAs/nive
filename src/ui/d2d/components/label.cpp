/// @file label.cpp
/// @brief Implementation of D2D label component

#include "label.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DLabel::D2DLabel() {
    focusable_ = false;
}

D2DLabel::D2DLabel(const std::wstring& text) : text_(text) {
    focusable_ = false;
}

void D2DLabel::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        text_layout_.Reset();
        invalidate();
    }
}

void D2DLabel::setTextAlignment(TextAlignment alignment) {
    if (text_alignment_ != alignment) {
        text_alignment_ = alignment;
        resources_dirty_ = true;
        invalidate();
    }
}

void D2DLabel::setVerticalAlignment(VerticalAlignment alignment) {
    if (vertical_alignment_ != alignment) {
        vertical_alignment_ = alignment;
        resources_dirty_ = true;
        invalidate();
    }
}

void D2DLabel::setTextColor(const Color& color) {
    if (!(text_color_ == color)) {
        text_color_ = color;
        text_brush_.Reset();
        invalidate();
    }
}

void D2DLabel::setFontSize(float size) {
    if (font_size_ != size) {
        font_size_ = size;
        resources_dirty_ = true;
        invalidate();
    }
}

void D2DLabel::setWordWrap(bool wrap) {
    if (word_wrap_ != wrap) {
        word_wrap_ = wrap;
        resources_dirty_ = true;
        invalidate();
    }
}

void D2DLabel::createResources(DeviceResources& resources) {
    if (resources_dirty_ || !text_format_) {
        updateTextFormat(resources);
        resources_dirty_ = false;
    }

    if (!text_brush_) {
        Color color = enabled_ ? text_color_ : Style::disabledTextColor();
        text_brush_ = resources.createSolidBrush(color);
    }
}

void D2DLabel::updateTextFormat(DeviceResources& resources) {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return;
    }

    DWRITE_TEXT_ALIGNMENT dw_alignment;
    switch (text_alignment_) {
    case TextAlignment::Center:
        dw_alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        break;
    case TextAlignment::Trailing:
        dw_alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        break;
    default:
        dw_alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        break;
    }

    DWRITE_PARAGRAPH_ALIGNMENT dw_para_alignment;
    switch (vertical_alignment_) {
    case VerticalAlignment::Center:
        dw_para_alignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
        break;
    case VerticalAlignment::Bottom:
        dw_para_alignment = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
        break;
    default:
        dw_para_alignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
        break;
    }

    text_format_.Reset();
    HRESULT hr = factory.dwriteFactory()->CreateTextFormat(
        Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, font_size_, L"", &text_format_);

    if (SUCCEEDED(hr) && text_format_) {
        text_format_->SetTextAlignment(dw_alignment);
        text_format_->SetParagraphAlignment(dw_para_alignment);
        text_format_->SetWordWrapping(word_wrap_ ? DWRITE_WORD_WRAPPING_WRAP
                                                 : DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    text_layout_.Reset();
}

void D2DLabel::updateTextLayout(float max_width, float max_height) {
    if (!text_format_ || text_.empty()) {
        text_layout_.Reset();
        return;
    }

    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return;
    }

    text_layout_.Reset();
    factory.dwriteFactory()->CreateTextLayout(text_.c_str(), static_cast<UINT32>(text_.length()),
                                              text_format_.Get(), max_width, max_height,
                                              &text_layout_);
}

void D2DLabel::arrange(const Rect& bounds) {
    if (bounds_.width != bounds.width || bounds_.height != bounds.height) {
        // Bounds changed, need to recreate text layout
        text_layout_.Reset();
    }
    D2DUIComponent::arrange(bounds);
}

Size D2DLabel::measure(const Size& available_size) {
    if (text_.empty()) {
        desired_size_ = {0.0f, font_size_};
        return desired_size_;
    }

    // Create a temporary layout to measure
    auto& factory = D2DFactory::instance();
    if (!factory.isValid() || !text_format_) {
        desired_size_ = {0.0f, font_size_};
        return desired_size_;
    }

    float max_width = word_wrap_ ? available_size.width : 10000.0f;
    float max_height = available_size.height > 0 ? available_size.height : 10000.0f;

    ComPtr<IDWriteTextLayout> layout;
    factory.dwriteFactory()->CreateTextLayout(text_.c_str(), static_cast<UINT32>(text_.length()),
                                              text_format_.Get(), max_width, max_height, &layout);

    if (layout) {
        DWRITE_TEXT_METRICS metrics;
        layout->GetMetrics(&metrics);
        desired_size_ = {metrics.widthIncludingTrailingWhitespace, metrics.height};
    } else {
        desired_size_ = {0.0f, font_size_};
    }

    return desired_size_;
}

void D2DLabel::render(ID2D1RenderTarget* rt) {
    if (!visible_ || text_.empty()) {
        return;
    }

    if (!text_brush_ || !text_format_) {
        return;
    }

    // Update brush color if disabled state changed
    if (!enabled_) {
        Color disabled_color = Style::disabledTextColor();
        text_brush_->SetColor(disabled_color.toD2D());
    } else {
        text_brush_->SetColor(text_color_.toD2D());
    }

    // Update or create text layout
    if (!text_layout_) {
        updateTextLayout(bounds_.width, bounds_.height);
    }

    if (text_layout_) {
        rt->DrawTextLayout(D2D1::Point2F(bounds_.x, bounds_.y), text_layout_.Get(),
                           text_brush_.Get());
    }
}

}  // namespace nive::ui::d2d
