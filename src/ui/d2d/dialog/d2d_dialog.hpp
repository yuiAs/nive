/// @file d2d_dialog.hpp
/// @brief Base class for D2D-rendered dialogs

#pragma once

#include <Windows.h>

#include <memory>
#include <string>
#include <vector>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/core/types.hpp"

namespace nive::ui::d2d {

/// @brief Base class for dialogs rendered with Direct2D
///
/// Creates a Win32 window that uses D2D for all rendering.
/// Components are added to the dialog and positioned using measure/arrange.
class D2DDialog : public D2DContainerComponent {
public:
    ~D2DDialog() override;

    D2DDialog(const D2DDialog&) = delete;
    D2DDialog& operator=(const D2DDialog&) = delete;

    /// @brief Show the dialog modally
    /// @param parent Parent window handle
    /// @return Dialog result (IDOK, IDCANCEL, etc.)
    INT_PTR showModal(HWND parent);

    /// @brief Get the dialog window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Get device resources
    [[nodiscard]] DeviceResources& deviceResources() noexcept { return device_resources_; }

    // D2DUIComponent overrides
    Size measure(const Size& available_size) override;

protected:
    D2DDialog();

    /// @brief Called when dialog is created (before WM_SHOWWINDOW)
    ///
    /// Override to create components, set initial size, etc.
    virtual void onCreate() {}

    /// @brief Called when dialog needs to render
    /// @param rt Render target
    virtual void onRender(ID2D1RenderTarget* rt);

    /// @brief Called when dialog is resized
    /// @param width New width in DIPs
    /// @param height New height in DIPs
    virtual void onResize(float width, float height);

    /// @brief Called when dialog is about to close
    /// @return true to allow close, false to cancel
    virtual bool onClose() { return true; }

    /// @brief End the dialog with a result
    void endDialog(INT_PTR result);

    /// @brief Get the dialog title
    [[nodiscard]] const std::wstring& title() const noexcept { return title_; }

    /// @brief Set the dialog title
    void setTitle(const std::wstring& title);

    /// @brief Set whether the dialog is resizable
    void setResizable(bool resizable) noexcept { resizable_ = resizable; }

    /// @brief Set the initial dialog size (in DIPs)
    void setInitialSize(const Size& size) noexcept { initial_size_ = size; }

    /// @brief Set the minimum dialog size (in DIPs)
    void setMinimumSize(const Size& size) noexcept { min_size_ = size; }

    /// @brief DPI scale a value
    [[nodiscard]] float scale(float value) const noexcept { return device_resources_.scale(value); }

    /// @brief DPI scale an integer value
    [[nodiscard]] int scale(int value) const noexcept { return device_resources_.scale(value); }

    /// @brief Request repaint when child components call invalidate()
    void onInvalidate() override {
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool createWindow(HWND parent);
    void destroyWindow();
    void updateLayout();

    // Event helpers
    [[nodiscard]] MouseEvent createMouseEvent(LPARAM lParam, WPARAM wParam,
                                              MouseButton button = MouseButton::None) const;
    [[nodiscard]] KeyEvent createKeyEvent(WPARAM wParam, LPARAM lParam) const;
    [[nodiscard]] Modifiers getCurrentModifiers() const;

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    DeviceResources device_resources_;
    INT_PTR result_ = IDCANCEL;

    std::wstring title_;
    Size initial_size_{400.0f, 300.0f};
    Size min_size_{200.0f, 150.0f};
    bool resizable_ = false;
    bool running_ = false;

    // Window class registration
    static ATOM window_class_;
    static bool registerWindowClass();
};

}  // namespace nive::ui::d2d
