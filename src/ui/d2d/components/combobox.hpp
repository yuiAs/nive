/// @file combobox.hpp
/// @brief D2D combo box (dropdown list) component

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

// Forward declaration for style traits
class D2DComboBox;

/// @brief Style traits for D2DComboBox
template <>
struct StyleTraits<D2DComboBox> {
    // Normal state
    static constexpr Color background() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color border() { return CommonStyle::border(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color arrowColor() { return CommonStyle::foreground(); }

    // Hover state
    static constexpr Color hoverBorder() { return CommonStyle::foreground(); }

    // Focused state
    static constexpr Color focusedBorder() { return CommonStyle::borderFocused(); }

    // Disabled state
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    // Dropdown list
    static constexpr Color dropdownBackground() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color dropdownBorder() { return CommonStyle::border(); }
    static constexpr Color itemHoverBackground() { return CommonStyle::hoverBackground(); }
    static constexpr Color selectedItemBackground() { return CommonStyle::accent(); }
    static constexpr Color selectedItemTextColor() { return Color::white(); }

    // Typography
    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }

    // Layout
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return CommonStyle::borderRadius(); }
    static constexpr Thickness padding() { return Thickness{8.0f, 4.0f, 8.0f, 4.0f}; }
    static constexpr float height() { return CommonStyle::controlHeight(); }
    static constexpr float arrowWidth() { return 20.0f; }
    static constexpr float itemHeight() { return 24.0f; }
    static constexpr float maxDropdownHeight() { return 200.0f; }
};

/// @brief Dropdown combo box component
///
/// Displays a dropdown list for single selection.
class D2DComboBox : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DComboBox>;
    using SelectionCallback = std::function<void(int index)>;

    D2DComboBox();
    ~D2DComboBox() override = default;

    // Item management

    /// @brief Add an item to the list
    void addItem(const std::wstring& text);

    /// @brief Add multiple items
    void addItems(const std::vector<std::wstring>& items);

    /// @brief Remove all items
    void clearItems();

    /// @brief Get item count
    [[nodiscard]] size_t itemCount() const noexcept { return items_.size(); }

    /// @brief Get item text at index
    [[nodiscard]] const std::wstring& itemAt(size_t index) const;

    // Selection

    /// @brief Get selected index (-1 if none)
    [[nodiscard]] int selectedIndex() const noexcept { return selected_index_; }

    /// @brief Set selected index
    void setSelectedIndex(int index);

    /// @brief Get selected item text (empty if none)
    [[nodiscard]] std::wstring selectedText() const;

    /// @brief Set selection callback
    void onSelectionChanged(SelectionCallback callback) {
        on_selection_changed_ = std::move(callback);
    }

    // Dropdown state

    /// @brief Check if dropdown is open
    [[nodiscard]] bool isDropdownOpen() const noexcept { return dropdown_open_; }

    /// @brief Open the dropdown list
    void openDropdown();

    /// @brief Close the dropdown list
    void closeDropdown();

    /// @brief Toggle dropdown state
    void toggleDropdown();

    /// @brief Get the dropdown rectangle (for popup rendering)
    /// @return Rectangle in parent coordinates where dropdown should appear
    [[nodiscard]] Rect dropdownRect() const;

    /// @brief Get the item under point in dropdown coordinates
    /// @param point Point relative to dropdown rect
    /// @return Item index or -1 if none
    [[nodiscard]] int hitTestDropdownItem(const Point& point) const;

    /// @brief Select item at dropdown point
    /// @param point Point relative to dropdown rect
    /// @return true if an item was selected
    bool selectDropdownItem(const Point& point);

    /// @brief Get hovered item index in dropdown
    [[nodiscard]] int hoveredItemIndex() const noexcept { return hovered_item_; }

    /// @brief Set hovered item index
    void setHoveredItem(int index);

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    /// @brief Render the dropdown list (called by dialog for popup layer)
    void renderDropdown(ID2D1RenderTarget* rt);

    bool onMouseEnter(const MouseEvent& event) override;
    bool onMouseLeave(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    void onFocusChanged(const FocusEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    void updateTextLayout();
    void renderArrow(ID2D1RenderTarget* rt, const Rect& arrow_rect);

    std::vector<std::wstring> items_;
    int selected_index_ = -1;
    int hovered_item_ = -1;
    bool dropdown_open_ = false;

    SelectionCallback on_selection_changed_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> background_brush_;
    ComPtr<ID2D1SolidColorBrush> border_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1SolidColorBrush> arrow_brush_;
    ComPtr<ID2D1SolidColorBrush> dropdown_background_brush_;
    ComPtr<ID2D1SolidColorBrush> dropdown_border_brush_;
    ComPtr<ID2D1SolidColorBrush> item_hover_brush_;
    ComPtr<ID2D1SolidColorBrush> selected_item_brush_;
    ComPtr<ID2D1SolidColorBrush> selected_item_text_brush_;

    static const std::wstring kEmptyString;
};

}  // namespace nive::ui::d2d
