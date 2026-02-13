/// @file device_resources.hpp
/// @brief HWND render target and device-dependent resource management

#pragma once

#include <Windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include "core/util/com_ptr.hpp"
#include "types.hpp"

namespace nive::ui::d2d {

/// @brief Manages Direct2D render target and device-dependent resources
///
/// Each window that uses D2D rendering should have its own DeviceResources
/// instance. Handles device lost scenarios and resource recreation.
class DeviceResources {
public:
    DeviceResources();
    ~DeviceResources() = default;

    DeviceResources(const DeviceResources&) = delete;
    DeviceResources& operator=(const DeviceResources&) = delete;

    /// @brief Set the target window
    /// @param hwnd Window handle to render to
    /// @return true if successful
    bool setTargetWindow(HWND hwnd);

    /// @brief Get the render target
    /// @return Render target, or nullptr if not initialized
    [[nodiscard]] ID2D1HwndRenderTarget* renderTarget() const noexcept {
        return render_target_.Get();
    }

    /// @brief Check if device resources are valid
    [[nodiscard]] bool isValid() const noexcept { return render_target_ != nullptr; }

    /// @brief Get the current DPI
    [[nodiscard]] float dpiX() const noexcept { return dpi_x_; }
    [[nodiscard]] float dpiY() const noexcept { return dpi_y_; }

    /// @brief Scale a value from base DPI (72) to current DPI
    /// @param value Value in base DPI units
    /// @return Scaled value in device-independent pixels
    [[nodiscard]] float scale(float value) const noexcept { return value * dpi_x_ / kBaseDpi; }

    /// @brief Scale an integer value from base DPI (72) to current DPI
    [[nodiscard]] int scale(int value) const noexcept {
        return static_cast<int>(static_cast<float>(value) * dpi_x_ / kBaseDpi + 0.5f);
    }

    /// @brief Begin drawing operations
    /// @return true if drawing can proceed
    bool beginDraw();

    /// @brief End drawing operations
    /// @return true if successful, false if device was lost (resources recreated)
    bool endDraw();

    /// @brief Resize the render target
    /// @param width New width in pixels
    /// @param height New height in pixels
    /// @return true if successful
    bool resize(uint32_t width, uint32_t height);

    /// @brief Handle DPI change
    /// @param dpi_x New horizontal DPI
    /// @param dpi_y New vertical DPI
    void setDpi(float dpi_x, float dpi_y);

    /// @brief Get the render target size in DIPs
    [[nodiscard]] Size getSize() const noexcept;

    /// @brief Discard device-dependent resources
    void discardResources();

    /// @brief Resource epoch counter, incremented on render target recreation
    [[nodiscard]] uint32_t resourceEpoch() const noexcept { return resource_epoch_; }

    /// @brief Create a solid color brush
    /// @param color Brush color
    /// @return Brush, or nullptr on failure
    [[nodiscard]] ComPtr<ID2D1SolidColorBrush> createSolidBrush(const Color& color);

    /// @brief Create a text format
    /// @param font_family Font family name
    /// @param font_size Font size in DIPs
    /// @param weight Font weight
    /// @param style Font style
    /// @return Text format, or nullptr on failure
    [[nodiscard]] ComPtr<IDWriteTextFormat>
    createTextFormat(const wchar_t* font_family, float font_size,
                     DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                     DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL);

private:
    bool createRenderTarget();

    static constexpr float kBaseDpi = 72.0f;

    HWND hwnd_ = nullptr;
    ComPtr<ID2D1HwndRenderTarget> render_target_;
    float dpi_x_ = 96.0f;
    float dpi_y_ = 96.0f;
    uint32_t resource_epoch_ = 0;
};

}  // namespace nive::ui::d2d
