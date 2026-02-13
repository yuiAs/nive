/// @file image_viewer_window.hpp
/// @brief Image viewer window for displaying images with zoom/pan support

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <d2d1.h>

#include <memory>
#include <optional>
#include <string>

#include "core/archive/virtual_path.hpp"
#include "core/config/settings.hpp"
#include "core/image/decoded_image.hpp"
#include "core/util/com_ptr.hpp"
#include "d2d/core/device_resources.hpp"

namespace nive::ui {

/// @brief Image viewer window with zoom/pan and display mode support
class ImageViewerWindow {
public:
    ImageViewerWindow();
    ~ImageViewerWindow();

    ImageViewerWindow(const ImageViewerWindow&) = delete;
    ImageViewerWindow& operator=(const ImageViewerWindow&) = delete;

    /// @brief Create the window
    /// @param hInstance Application instance
    /// @return true if created successfully
    bool create(HINSTANCE hInstance);

    /// @brief Show the window
    void show();

    /// @brief Close the window
    void close();

    /// @brief Check if window is visible
    [[nodiscard]] bool isVisible() const noexcept;

    /// @brief Get window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Set image to display
    /// @param path Virtual path to the image
    void setImage(const archive::VirtualPath& path);

    /// @brief Set display mode
    /// @param mode Display mode
    void setDisplayMode(config::ViewerDisplayMode mode);

    /// @brief Get current display mode
    [[nodiscard]] config::ViewerDisplayMode displayMode() const noexcept { return display_mode_; }

    /// @brief Zoom in
    void zoomIn();

    /// @brief Zoom out
    void zoomOut();

    /// @brief Reset zoom to 100%
    void zoomReset();

    /// @brief Fit image to window
    void fitToWindow();

    /// @brief Navigate to next image
    void nextImage();

    /// @brief Navigate to previous image
    void previousImage();

    /// @brief Save window state to settings
    void saveState(config::Settings& settings) const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void onCreate();
    void onSize(int width, int height);
    void onPaint();
    void onDestroy();
    void onKeyDown(WPARAM vk);
    void onMouseWheel(int delta, int x, int y);
    void onLbuttonDown(int x, int y);
    void onLbuttonUp(int x, int y);
    void onMouseMove(int x, int y);
    void onLbuttonDblclk(int x, int y);

    void render();
    void recreateBitmap();
    void updateTitle();
    void clampScroll();
    void centerImage();
    void createMenu();
    void onCommand(WORD id);
    void updateMenuCheck();

    /// @brief Calculate zoom factor based on current display mode
    [[nodiscard]] float calculateFitZoom(int image_width, int image_height, int view_width,
                                         int view_height) const;

    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    HMENU menu_ = nullptr;

    // D2D rendering
    d2d::DeviceResources device_resources_;
    ComPtr<ID2D1Bitmap> bitmap_;
    uint32_t last_resource_epoch_ = 0;

    // Current image
    std::unique_ptr<image::DecodedImage> image_;
    archive::VirtualPath current_path_;

    // Display settings
    config::ViewerDisplayMode display_mode_ = config::ViewerDisplayMode::ShrinkToFit;
    float zoom_ = 1.0f;
    int scroll_x_ = 0;
    int scroll_y_ = 0;

    // Drag state for panning
    bool dragging_ = false;
    int drag_start_x_ = 0;
    int drag_start_y_ = 0;
    int drag_start_scroll_x_ = 0;
    int drag_start_scroll_y_ = 0;

    // Zoom limits
    static constexpr float kMinZoom = 0.1f;    // 10%
    static constexpr float kMaxZoom = 32.0f;   // 3200%
    static constexpr float kZoomStep = 1.25f;  // 25% per step

    // Menu IDs
    static constexpr WORD kIdFileExit = 1001;
    static constexpr WORD kIdViewOriginal = 1101;
    static constexpr WORD kIdViewFitToWindow = 1102;
    static constexpr WORD kIdViewShrinkToFit = 1103;
    static constexpr WORD kIdViewZoomIn = 1111;
    static constexpr WORD kIdViewZoomOut = 1112;
    static constexpr WORD kIdViewResetZoom = 1113;
};

}  // namespace nive::ui
