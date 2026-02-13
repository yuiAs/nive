/// @file label.hpp
/// @brief D2D label component for text display

#pragma once

#include <dwrite.h>

#include <string>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Text alignment options
enum class TextAlignment {
    Leading,  // Left for LTR, Right for RTL
    Center,
    Trailing,  // Right for LTR, Left for RTL
};

/// @brief Vertical text alignment
enum class VerticalAlignment {
    Top,
    Center,
    Bottom,
};

/// @brief Simple text label component
///
/// Displays static text using DirectWrite.
class D2DLabel : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DLabel>;

    D2DLabel();
    explicit D2DLabel(const std::wstring& text);
    ~D2DLabel() override = default;

    // Properties

    [[nodiscard]] const std::wstring& text() const noexcept { return text_; }
    void setText(const std::wstring& text);

    [[nodiscard]] TextAlignment textAlignment() const noexcept { return text_alignment_; }
    void setTextAlignment(TextAlignment alignment);

    [[nodiscard]] VerticalAlignment verticalAlignment() const noexcept {
        return vertical_alignment_;
    }
    void setVerticalAlignment(VerticalAlignment alignment);

    [[nodiscard]] const Color& textColor() const noexcept { return text_color_; }
    void setTextColor(const Color& color);

    [[nodiscard]] float fontSize() const noexcept { return font_size_; }
    void setFontSize(float size);

    [[nodiscard]] bool wordWrap() const noexcept { return word_wrap_; }
    void setWordWrap(bool wrap);

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void arrange(const Rect& bounds) override;
    void render(ID2D1RenderTarget* rt) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    void updateTextFormat(DeviceResources& resources);
    void updateTextLayout(float max_width, float max_height);

    std::wstring text_;
    Color text_color_ = Style::textColor();
    float font_size_ = Style::fontSize();
    TextAlignment text_alignment_ = TextAlignment::Leading;
    VerticalAlignment vertical_alignment_ = VerticalAlignment::Top;
    bool word_wrap_ = false;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    bool resources_dirty_ = true;
};

}  // namespace nive::ui::d2d
