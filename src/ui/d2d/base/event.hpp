/// @file event.hpp
/// @brief Event definitions for D2D UI components

#pragma once

#include <cstdint>

#include "ui/d2d/core/types.hpp"

namespace nive::ui::d2d {

/// @brief Mouse button identifiers
enum class MouseButton : uint8_t {
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
};

/// @brief Keyboard modifier flags
enum class Modifiers : uint8_t {
    None = 0,
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) noexcept {
    return static_cast<Modifiers>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr Modifiers operator&(Modifiers a, Modifiers b) noexcept {
    return static_cast<Modifiers>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

constexpr bool hasModifier(Modifiers value, Modifiers flag) noexcept {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

/// @brief Base event structure
struct Event {
    bool handled = false;  // Set to true to stop event propagation
};

/// @brief Mouse event
struct MouseEvent : Event {
    Point position;        // Position relative to component
    Point screenPosition;  // Position in screen coordinates
    MouseButton button = MouseButton::None;
    Modifiers modifiers = Modifiers::None;
    int clickCount = 0;  // 1 = single click, 2 = double click
    int wheelDelta = 0;  // Mouse wheel delta (positive = up)
};

/// @brief Key event
struct KeyEvent : Event {
    uint32_t keyCode = 0;   // Virtual key code
    wchar_t character = 0;  // Character for WM_CHAR
    Modifiers modifiers = Modifiers::None;
    bool repeat = false;  // Key repeat from held key
};

/// @brief Focus event
struct FocusEvent : Event {
    bool gained = false;  // true = focus gained, false = focus lost
};

/// @brief State change event for components that track state
struct StateChangeEvent : Event {
    bool checked = false;    // For checkbox/radio
    int selectedIndex = -1;  // For lists, combo boxes
};

/// @brief Text change event
struct TextChangeEvent : Event {
    // Text is retrieved from the component directly
};

}  // namespace nive::ui::d2d
