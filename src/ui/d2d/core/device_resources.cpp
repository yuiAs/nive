/// @file device_resources.cpp
/// @brief Implementation of device resources management

#include "device_resources.hpp"

#include "core/util/logger.hpp"
#include "d2d_factory.hpp"

namespace nive::ui::d2d {

DeviceResources::DeviceResources() {
    // Get system DPI
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        dpi_x_ = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSX));
        dpi_y_ = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSY));
        ReleaseDC(nullptr, hdc);
    }
}

bool DeviceResources::setTargetWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_ERROR("Invalid window handle");
        return false;
    }

    hwnd_ = hwnd;

    // Get window DPI if available (Windows 10+)
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi > 0) {
        dpi_x_ = static_cast<float>(dpi);
        dpi_y_ = static_cast<float>(dpi);
    }

    return createRenderTarget();
}

bool DeviceResources::createRenderTarget() {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        LOG_ERROR("D2D factory not available");
        return false;
    }

    // Discard existing target
    render_target_.Reset();

    RECT rc;
    GetClientRect(hwnd_, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(rc.right - rc.left),
                                   static_cast<UINT32>(rc.bottom - rc.top));

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.dpiX = dpi_x_;
    props.dpiY = dpi_y_;

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props = D2D1::HwndRenderTargetProperties(hwnd_, size);

    HRESULT hr = factory.d2dFactory()->CreateHwndRenderTarget(props, hwnd_props, &render_target_);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create HWND render target: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    LOG_DEBUG("D2D render target created ({}x{} @ {:.0f} DPI)", size.width, size.height, dpi_x_);
    return true;
}

bool DeviceResources::beginDraw() {
    if (!render_target_) {
        return false;
    }
    render_target_->BeginDraw();
    return true;
}

bool DeviceResources::endDraw() {
    if (!render_target_) {
        return false;
    }

    HRESULT hr = render_target_->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        // Device lost - recreate resources
        LOG_WARN("D2D device lost, recreating resources");
        discardResources();
        if (createRenderTarget()) {
            ++resource_epoch_;
            // Invalidate to trigger repaint with new resources
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return false;
    }

    if (FAILED(hr)) {
        LOG_ERROR("EndDraw failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

bool DeviceResources::resize(uint32_t width, uint32_t height) {
    if (!render_target_) {
        return false;
    }

    D2D1_SIZE_U size = D2D1::SizeU(width, height);
    HRESULT hr = render_target_->Resize(size);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to resize render target: 0x{:08X}", static_cast<unsigned>(hr));
        // Try recreating the render target
        return createRenderTarget();
    }

    return true;
}

void DeviceResources::setDpi(float dpi_x, float dpi_y) {
    if (dpi_x_ == dpi_x && dpi_y_ == dpi_y) {
        return;
    }

    dpi_x_ = dpi_x;
    dpi_y_ = dpi_y;

    if (render_target_) {
        render_target_->SetDpi(dpi_x_, dpi_y_);
    }
}

Size DeviceResources::getSize() const noexcept {
    if (!render_target_) {
        return {};
    }
    auto size = render_target_->GetSize();
    return {size.width, size.height};
}

void DeviceResources::discardResources() {
    render_target_.Reset();
}

ComPtr<ID2D1SolidColorBrush> DeviceResources::createSolidBrush(const Color& color) {
    if (!render_target_) {
        return nullptr;
    }

    ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = render_target_->CreateSolidColorBrush(color.toD2D(), &brush);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create solid brush: 0x{:08X}", static_cast<unsigned>(hr));
        return nullptr;
    }

    return brush;
}

ComPtr<IDWriteTextFormat> DeviceResources::createTextFormat(const wchar_t* font_family,
                                                            float font_size,
                                                            DWRITE_FONT_WEIGHT weight,
                                                            DWRITE_FONT_STYLE style) {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return nullptr;
    }

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = factory.dwriteFactory()->CreateTextFormat(
        font_family,
        nullptr,  // font collection (nullptr = system fonts)
        weight, style, DWRITE_FONT_STRETCH_NORMAL, font_size,
        L"",  // locale name
        &format);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create text format: 0x{:08X}", static_cast<unsigned>(hr));
        return nullptr;
    }

    return format;
}

}  // namespace nive::ui::d2d
