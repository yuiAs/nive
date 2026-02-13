/// @file default_style.hpp
/// @brief Type traits based default styles for D2D UI components

#pragma once

#include "ui/d2d/core/types.hpp"

namespace nive::ui::d2d {

// Forward declarations
class D2DLabel;
class D2DButton;
class D2DCheckBox;
class D2DRadioButton;
class D2DGroupBox;
class D2DEditBox;

/// @brief Default style traits for UI components
///
/// Specialize this template to define styles for each component type.
template <typename Component>
struct StyleTraits;

/// @brief Common style values shared across components
struct CommonStyle {
    // Colors - Windows 11 inspired
    static constexpr Color background() { return Color::fromRgb(0xFBFBFB); }
    static constexpr Color foreground() { return Color::fromRgb(0x1A1A1A); }
    static constexpr Color accent() { return Color::fromRgb(0x0078D4); }
    static constexpr Color accentHover() { return Color::fromRgb(0x1982D4); }
    static constexpr Color accentPressed() { return Color::fromRgb(0x006CBE); }
    static constexpr Color border() { return Color::fromRgb(0xD1D1D1); }
    static constexpr Color borderFocused() { return Color::fromRgb(0x0078D4); }
    static constexpr Color disabled() { return Color::fromRgb(0xA0A0A0); }
    static constexpr Color disabledBackground() { return Color::fromRgb(0xF0F0F0); }
    static constexpr Color hoverBackground() { return Color::fromRgb(0xE5E5E5); }
    static constexpr Color pressedBackground() { return Color::fromRgb(0xCCCCCC); }

    // Typography
    static constexpr const wchar_t* fontFamily() { return L"Segoe UI"; }
    static constexpr float fontSize() { return 12.0f; }
    static constexpr float headerFontSize() { return 14.0f; }

    // Spacing
    static constexpr float controlHeight() { return 24.0f; }
    static constexpr float borderRadius() { return 4.0f; }
    static constexpr Thickness padding() { return Thickness{8.0f, 4.0f, 8.0f, 4.0f}; }
};

/// @brief Style traits for D2DLabel
template <>
struct StyleTraits<D2DLabel> {
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }
    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
};

/// @brief Style traits for D2DButton
template <>
struct StyleTraits<D2DButton> {
    // Normal state
    static constexpr Color background() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color border() { return CommonStyle::border(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }

    // Hover state
    static constexpr Color hoverBackground() { return CommonStyle::hoverBackground(); }
    static constexpr Color hoverBorder() { return CommonStyle::border(); }

    // Pressed state
    static constexpr Color pressedBackground() { return CommonStyle::pressedBackground(); }
    static constexpr Color pressedBorder() { return CommonStyle::border(); }

    // Focused state
    static constexpr Color focusedBorder() { return CommonStyle::borderFocused(); }

    // Disabled state
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    // Primary (accent) button variant
    static constexpr Color primaryBackground() { return CommonStyle::accent(); }
    static constexpr Color primaryHoverBackground() { return CommonStyle::accentHover(); }
    static constexpr Color primaryPressedBackground() { return CommonStyle::accentPressed(); }
    static constexpr Color primaryTextColor() { return Color::white(); }

    // Typography
    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }

    // Layout
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return CommonStyle::borderRadius(); }
    static constexpr Thickness padding() { return CommonStyle::padding(); }
    static constexpr float minWidth() { return 75.0f; }
    static constexpr float height() { return CommonStyle::controlHeight(); }
};

/// @brief Style traits for D2DCheckBox
template <>
struct StyleTraits<D2DCheckBox> {
    static constexpr Color boxBackground() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color boxBorder() { return CommonStyle::border(); }
    static constexpr Color boxHoverBorder() { return CommonStyle::foreground(); }
    static constexpr Color checkedBackground() { return CommonStyle::accent(); }
    static constexpr Color checkedBorder() { return CommonStyle::accent(); }
    static constexpr Color checkmarkColor() { return Color::white(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
    static constexpr float boxSize() { return 16.0f; }
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return 3.0f; }
    static constexpr float spacing() { return 8.0f; }  // Space between box and text
};

/// @brief Style traits for D2DRadioButton
template <>
struct StyleTraits<D2DRadioButton> {
    static constexpr Color circleBackground() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color circleBorder() { return CommonStyle::border(); }
    static constexpr Color circleHoverBorder() { return CommonStyle::foreground(); }
    static constexpr Color selectedFill() { return CommonStyle::accent(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
    static constexpr float circleSize() { return 16.0f; }
    static constexpr float innerCircleSize() { return 8.0f; }
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float spacing() { return 8.0f; }
};

/// @brief Style traits for D2DGroupBox
template <>
struct StyleTraits<D2DGroupBox> {
    static constexpr Color border() { return CommonStyle::border(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }
    // Background color used to "erase" border behind label text
    static constexpr Color labelBackground() { return CommonStyle::background(); }

    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return CommonStyle::borderRadius(); }
    static constexpr Thickness padding() { return Thickness{12.0f, 20.0f, 12.0f, 12.0f}; }
};

/// @brief Style traits for D2DEditBox
template <>
struct StyleTraits<D2DEditBox> {
    static constexpr Color background() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color border() { return CommonStyle::border(); }
    static constexpr Color focusedBorder() { return CommonStyle::borderFocused(); }
    static constexpr Color hoverBorder() { return CommonStyle::foreground(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }
    static constexpr Color placeholderColor() { return Color::fromRgb(0x808080); }
    static constexpr Color selectionBackground() { return CommonStyle::accent().withAlpha(0.3f); }
    static constexpr Color caretColor() { return CommonStyle::foreground(); }
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return CommonStyle::borderRadius(); }
    static constexpr Thickness padding() { return Thickness{8.0f, 4.0f, 8.0f, 4.0f}; }
    static constexpr float height() { return CommonStyle::controlHeight(); }
    static constexpr float caretWidth() { return 1.0f; }
};

}  // namespace nive::ui::d2d
