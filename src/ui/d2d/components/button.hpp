/// @file button.hpp
/// @brief D2D button component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <string>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Button visual state
enum class ButtonState {
    Normal,
    Hover,
    Pressed,
    Disabled,
};

/// @brief Button style variant
enum class ButtonVariant {
    Standard,  // Normal button with border
    Primary,   // Accent-colored button
};

/// @brief Clickable button component
///
/// Renders a button with text and responds to mouse clicks.
class D2DButton : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DButton>;
    using ClickCallback = std::function<void()>;

    D2DButton();
    explicit D2DButton(const std::wstring& text);
    ~D2DButton() override = default;

    // Properties

    [[nodiscard]] const std::wstring& text() const noexcept { return text_; }
    void setText(const std::wstring& text);

    [[nodiscard]] ButtonVariant variant() const noexcept { return variant_; }
    void setVariant(ButtonVariant variant);

    /// @brief Set click callback
    void onClick(ClickCallback callback) { on_click_ = std::move(callback); }

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseEnter(const MouseEvent& event) override;
    bool onMouseLeave(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    [[nodiscard]] ButtonState currentState() const noexcept;
    void updateBrushes(DeviceResources& resources);

    std::wstring text_;
    ButtonVariant variant_ = ButtonVariant::Standard;
    bool pressed_ = false;
    ClickCallback on_click_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;

    // Brushes for different states
    ComPtr<ID2D1SolidColorBrush> background_brush_;
    ComPtr<ID2D1SolidColorBrush> border_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;

    bool resources_dirty_ = true;
};

}  // namespace nive::ui::d2d
