/// @file types.hpp
/// @brief Basic types for D2D UI framework

#pragma once

#include <d2d1.h>

#include <algorithm>
#include <cstdint>

namespace nive::ui::d2d {

/// @brief 2D point in device-independent pixels (DIPs)
struct Point {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Point() noexcept = default;
    constexpr Point(float x, float y) noexcept : x(x), y(y) {}
    constexpr Point(int x, int y) noexcept : x(static_cast<float>(x)), y(static_cast<float>(y)) {}

    constexpr Point operator+(const Point& other) const noexcept {
        return {x + other.x, y + other.y};
    }

    constexpr Point operator-(const Point& other) const noexcept {
        return {x - other.x, y - other.y};
    }

    constexpr bool operator==(const Point& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] D2D1_POINT_2F toD2D() const noexcept { return D2D1::Point2F(x, y); }
};

/// @brief 2D size in device-independent pixels (DIPs)
struct Size {
    float width = 0.0f;
    float height = 0.0f;

    constexpr Size() noexcept = default;
    constexpr Size(float w, float h) noexcept : width(w), height(h) {}
    constexpr Size(int w, int h) noexcept
        : width(static_cast<float>(w)), height(static_cast<float>(h)) {}

    constexpr bool operator==(const Size& other) const noexcept {
        return width == other.width && height == other.height;
    }

    [[nodiscard]] constexpr bool isEmpty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] D2D1_SIZE_F toD2D() const noexcept { return D2D1::SizeF(width, height); }
};

/// @brief Rectangle in device-independent pixels (DIPs)
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rect() noexcept = default;
    constexpr Rect(float x, float y, float w, float h) noexcept : x(x), y(y), width(w), height(h) {}
    constexpr Rect(int x, int y, int w, int h) noexcept
        : x(static_cast<float>(x)), y(static_cast<float>(y)), width(static_cast<float>(w)),
          height(static_cast<float>(h)) {}
    constexpr Rect(const Point& pos, const Size& size) noexcept
        : x(pos.x), y(pos.y), width(size.width), height(size.height) {}

    [[nodiscard]] constexpr float left() const noexcept { return x; }
    [[nodiscard]] constexpr float top() const noexcept { return y; }
    [[nodiscard]] constexpr float right() const noexcept { return x + width; }
    [[nodiscard]] constexpr float bottom() const noexcept { return y + height; }

    [[nodiscard]] constexpr Point position() const noexcept { return {x, y}; }
    [[nodiscard]] constexpr Size size() const noexcept { return {width, height}; }
    [[nodiscard]] constexpr Point center() const noexcept {
        return {x + width / 2.0f, y + height / 2.0f};
    }

    [[nodiscard]] constexpr bool isEmpty() const noexcept {
        return width <= 0.0f || height <= 0.0f;
    }

    [[nodiscard]] constexpr bool contains(const Point& pt) const noexcept {
        return pt.x >= x && pt.x < x + width && pt.y >= y && pt.y < y + height;
    }

    [[nodiscard]] constexpr bool contains(float px, float py) const noexcept {
        return contains(Point{px, py});
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const noexcept {
        return x < other.right() && right() > other.x && y < other.bottom() && bottom() > other.y;
    }

    [[nodiscard]] constexpr Rect intersected(const Rect& other) const noexcept {
        float l = std::max(x, other.x);
        float t = std::max(y, other.y);
        float r = std::min(right(), other.right());
        float b = std::min(bottom(), other.bottom());
        if (l >= r || t >= b)
            return {};
        return {l, t, r - l, b - t};
    }

    [[nodiscard]] constexpr Rect united(const Rect& other) const noexcept {
        if (isEmpty())
            return other;
        if (other.isEmpty())
            return *this;
        float l = std::min(x, other.x);
        float t = std::min(y, other.y);
        float r = std::max(right(), other.right());
        float b = std::max(bottom(), other.bottom());
        return {l, t, r - l, b - t};
    }

    [[nodiscard]] constexpr Rect inflated(float dx, float dy) const noexcept {
        return {x - dx, y - dy, width + dx * 2, height + dy * 2};
    }

    [[nodiscard]] constexpr Rect deflated(float dx, float dy) const noexcept {
        return inflated(-dx, -dy);
    }

    [[nodiscard]] constexpr Rect translated(float dx, float dy) const noexcept {
        return {x + dx, y + dy, width, height};
    }

    [[nodiscard]] constexpr Rect translated(const Point& offset) const noexcept {
        return translated(offset.x, offset.y);
    }

    constexpr bool operator==(const Rect& other) const noexcept {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }

    [[nodiscard]] D2D1_RECT_F toD2D() const noexcept {
        return D2D1::RectF(x, y, x + width, y + height);
    }

    [[nodiscard]] static constexpr Rect fromD2D(const D2D1_RECT_F& rc) noexcept {
        return {rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top};
    }
};

/// @brief RGBA color with float components [0.0, 1.0]
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    constexpr Color() noexcept = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) noexcept : r(r), g(g), b(b), a(a) {}

    /// @brief Create color from 8-bit components [0-255]
    [[nodiscard]] static constexpr Color fromRgb(uint8_t r, uint8_t g, uint8_t b,
                                                 uint8_t a = 255) noexcept {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    /// @brief Create color from 32-bit ARGB value (0xAARRGGBB)
    [[nodiscard]] static constexpr Color fromArgb(uint32_t argb) noexcept {
        return fromRgb((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
    }

    /// @brief Create color from 32-bit RGB value (0xRRGGBB), alpha = 1.0
    [[nodiscard]] static constexpr Color fromRgb(uint32_t rgb) noexcept {
        return fromRgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    [[nodiscard]] constexpr Color withAlpha(float alpha) const noexcept { return {r, g, b, alpha}; }

    constexpr bool operator==(const Color& other) const noexcept {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    [[nodiscard]] D2D1_COLOR_F toD2D() const noexcept { return D2D1::ColorF(r, g, b, a); }

    // Common colors
    [[nodiscard]] static constexpr Color transparent() noexcept { return {0, 0, 0, 0}; }
    [[nodiscard]] static constexpr Color black() noexcept { return {0, 0, 0, 1}; }
    [[nodiscard]] static constexpr Color white() noexcept { return {1, 1, 1, 1}; }
    [[nodiscard]] static constexpr Color red() noexcept { return {1, 0, 0, 1}; }
    [[nodiscard]] static constexpr Color green() noexcept { return {0, 1, 0, 1}; }
    [[nodiscard]] static constexpr Color blue() noexcept { return {0, 0, 1, 1}; }
    [[nodiscard]] static constexpr Color gray() noexcept { return {0.5f, 0.5f, 0.5f, 1}; }
    [[nodiscard]] static constexpr Color lightGray() noexcept { return {0.75f, 0.75f, 0.75f, 1}; }
    [[nodiscard]] static constexpr Color darkGray() noexcept { return {0.25f, 0.25f, 0.25f, 1}; }
};

/// @brief Thickness values for margins and padding (left, top, right, bottom)
struct Thickness {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    constexpr Thickness() noexcept = default;

    constexpr explicit Thickness(float uniform) noexcept
        : left(uniform), top(uniform), right(uniform), bottom(uniform) {}

    constexpr Thickness(float horizontal, float vertical) noexcept
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}

    constexpr Thickness(float l, float t, float r, float b) noexcept
        : left(l), top(t), right(r), bottom(b) {}

    [[nodiscard]] constexpr float horizontalTotal() const noexcept { return left + right; }

    [[nodiscard]] constexpr float verticalTotal() const noexcept { return top + bottom; }

    constexpr bool operator==(const Thickness& other) const noexcept {
        return left == other.left && top == other.top && right == other.right &&
               bottom == other.bottom;
    }
};

}  // namespace nive::ui::d2d
