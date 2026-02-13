/// @file radiobutton.hpp
/// @brief D2D radio button component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

class D2DRadioGroup;

/// @brief Radio button component with mutually exclusive selection
///
/// Radio buttons in the same group are mutually exclusive.
/// Only one can be selected at a time.
class D2DRadioButton : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DRadioButton>;
    using ChangeCallback = std::function<void(bool selected)>;

    D2DRadioButton();
    explicit D2DRadioButton(const std::wstring& text);
    ~D2DRadioButton() override = default;

    // Properties

    [[nodiscard]] const std::wstring& text() const noexcept { return text_; }
    void setText(const std::wstring& text);

    [[nodiscard]] bool isSelected() const noexcept { return selected_; }
    void setSelected(bool selected);

    [[nodiscard]] D2DRadioGroup* group() const noexcept { return group_; }
    void setGroup(D2DRadioGroup* group);

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
    void select();
    [[nodiscard]] Rect getCircleRect() const noexcept;

    std::wstring text_;
    bool selected_ = false;
    bool pressed_ = false;
    D2DRadioGroup* group_ = nullptr;
    ChangeCallback on_change_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> circle_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> circle_border_brush_;
    ComPtr<ID2D1SolidColorBrush> selected_fill_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
};

/// @brief Group for managing mutually exclusive radio buttons
class D2DRadioGroup {
public:
    D2DRadioGroup() = default;
    ~D2DRadioGroup() = default;

    /// @brief Add a radio button to this group
    void addButton(D2DRadioButton* button);

    /// @brief Remove a radio button from this group
    void removeButton(D2DRadioButton* button);

    /// @brief Get the currently selected button
    [[nodiscard]] D2DRadioButton* selectedButton() const noexcept { return selected_; }

    /// @brief Called by a radio button when it wants to be selected
    void onButtonSelected(D2DRadioButton* button);

    /// @brief Update selection tracking (called by setSelected)
    ///
    /// Unlike onButtonSelected, this handles the case where setSelected
    /// is called directly. Deselects previous button if needed.
    void updateSelectionTracking(D2DRadioButton* button, bool selected);

private:
    std::vector<D2DRadioButton*> buttons_;
    D2DRadioButton* selected_ = nullptr;
};

}  // namespace nive::ui::d2d
