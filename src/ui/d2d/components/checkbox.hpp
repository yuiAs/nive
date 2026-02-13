/// @file checkbox.hpp
/// @brief D2D checkbox component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <string>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Checkbox component with check/uncheck state
class D2DCheckBox : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DCheckBox>;
    using ChangeCallback = std::function<void(bool checked)>;

    D2DCheckBox();
    explicit D2DCheckBox(const std::wstring& text);
    ~D2DCheckBox() override = default;

    // Properties

    [[nodiscard]] const std::wstring& text() const noexcept { return text_; }
    void setText(const std::wstring& text);

    [[nodiscard]] bool isChecked() const noexcept { return checked_; }
    void setChecked(bool checked);

    /// @brief Set change callback
    void onChange(ChangeCallback callback) { on_change_ = std::move(callback); }

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    void toggle();
    [[nodiscard]] Rect getBoxRect() const noexcept;

    std::wstring text_;
    bool checked_ = false;
    bool pressed_ = false;
    ChangeCallback on_change_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> box_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> box_border_brush_;
    ComPtr<ID2D1SolidColorBrush> checkmark_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
};

}  // namespace nive::ui::d2d
