/// @file spinedit.hpp
/// @brief D2D spin edit (numeric input with up/down buttons) component

#pragma once

#include <functional>
#include <string>

#include "button.hpp"
#include "editbox.hpp"

namespace nive::ui::d2d {

/// @brief Numeric input with increment/decrement buttons
///
/// Combines a text edit with up/down buttons for numeric input.
class D2DSpinEdit : public D2DUIComponent {
public:
    using ChangeCallback = std::function<void(int value)>;

    D2DSpinEdit();
    ~D2DSpinEdit() override = default;

    // Properties

    [[nodiscard]] int value() const noexcept { return value_; }
    void setValue(int value);

    [[nodiscard]] int minimum() const noexcept { return min_value_; }
    void setMinimum(int min);

    [[nodiscard]] int maximum() const noexcept { return max_value_; }
    void setMaximum(int max);

    void setRange(int min, int max);

    [[nodiscard]] int step() const noexcept { return step_; }
    void setStep(int step);

    /// @brief Set value change callback
    void onChange(ChangeCallback callback) { on_change_ = std::move(callback); }

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void arrange(const Rect& bounds) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onChar(const KeyEvent& event) override;
    void onFocusChanged(const FocusEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    void increment();
    void decrement();
    void updateEditText();
    void parseEditText();
    void clampValue();

    int value_ = 0;
    int min_value_ = 0;
    int max_value_ = 100;
    int step_ = 1;

    ChangeCallback on_change_;

    // Child components (owned)
    std::unique_ptr<D2DEditBox> edit_;

    // Button state
    bool up_pressed_ = false;
    bool down_pressed_ = false;
    bool up_hovered_ = false;
    bool down_hovered_ = false;

    // Button bounds
    Rect up_button_bounds_;
    Rect down_button_bounds_;

    // Brushes for buttons
    ComPtr<ID2D1SolidColorBrush> button_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> button_hover_brush_;
    ComPtr<ID2D1SolidColorBrush> button_pressed_brush_;
    ComPtr<ID2D1SolidColorBrush> button_border_brush_;
    ComPtr<ID2D1SolidColorBrush> arrow_brush_;

    static constexpr float kButtonWidth = 20.0f;
};

}  // namespace nive::ui::d2d
