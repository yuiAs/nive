/// @file d2d_dialog.cpp
/// @brief Implementation of D2D dialog base class

#include "d2d_dialog.hpp"

#include <windowsx.h>

#include <algorithm>

#include "core/util/logger.hpp"
#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/radiobutton.hpp"
#include "ui/d2d/components/tabcontrol.hpp"

namespace nive::ui::d2d {

ATOM D2DDialog::window_class_ = 0;

bool D2DDialog::registerWindowClass() {
    if (window_class_ != 0) {
        return true;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // No background - we render with D2D
    wc.lpszClassName = L"NiveD2DDialog";

    window_class_ = RegisterClassExW(&wc);
    if (!window_class_) {
        LOG_ERROR("Failed to register D2D dialog window class: {}", GetLastError());
        return false;
    }

    return true;
}

D2DDialog::D2DDialog() = default;

D2DDialog::~D2DDialog() {
    destroyWindow();
}

INT_PTR D2DDialog::showModal(HWND parent) {
    parent_ = parent;
    result_ = IDCANCEL;

    if (!createWindow(parent)) {
        return IDCANCEL;
    }

    // Disable parent window
    if (parent) {
        EnableWindow(parent, FALSE);
    }

    running_ = true;

    // Message loop
    MSG msg;
    while (running_ && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Re-enable parent window
    if (parent) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }

    destroyWindow();

    return result_;
}

bool D2DDialog::createWindow(HWND parent) {
    if (!registerWindowClass()) {
        return false;
    }

    // Calculate window style
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    if (resizable_) {
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }

    // Get DPI for initial size calculation
    UINT dpi = 96;
    if (parent) {
        dpi = GetDpiForWindow(parent);
    } else {
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            dpi = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
            ReleaseDC(nullptr, hdc);
        }
    }

    float dpi_scale = static_cast<float>(dpi) / 72.0f;

    // Calculate window size from client size
    RECT rc = {0, 0, static_cast<LONG>(initial_size_.width * dpi_scale),
               static_cast<LONG>(initial_size_.height * dpi_scale)};
    AdjustWindowRect(&rc, style, FALSE);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Center on parent or screen
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (parent) {
        RECT parent_rc;
        GetWindowRect(parent, &parent_rc);
        x = parent_rc.left + (parent_rc.right - parent_rc.left - width) / 2;
        y = parent_rc.top + (parent_rc.bottom - parent_rc.top - height) / 2;
    }

    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME, MAKEINTATOM(window_class_), title_.c_str(), style,
                            x, y, width, height, parent, nullptr, GetModuleHandleW(nullptr), this);

    if (!hwnd_) {
        LOG_ERROR("Failed to create D2D dialog window: {}", GetLastError());
        return false;
    }

    // Initialize D2D resources
    if (!device_resources_.setTargetWindow(hwnd_)) {
        LOG_ERROR("Failed to initialize D2D resources");
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    // Call onCreate for subclass initialization
    onCreate();

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    // Set initial focus to the first focusable component
    advanceFocus(true);

    return true;
}

void D2DDialog::destroyWindow() {
    if (hwnd_) {
        device_resources_.discardResources();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void D2DDialog::endDialog(INT_PTR result) {
    result_ = result;
    running_ = false;
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void D2DDialog::setTitle(const std::wstring& title) {
    title_ = title;
    if (hwnd_) {
        SetWindowTextW(hwnd_, title_.c_str());
    }
}

LRESULT CALLBACK D2DDialog::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    D2DDialog* dialog = nullptr;

    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        dialog = static_cast<D2DDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
        dialog->hwnd_ = hwnd;
    } else {
        dialog = reinterpret_cast<D2DDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (dialog) {
        return dialog->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT D2DDialog::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);

        if (device_resources_.beginDraw()) {
            auto rt = device_resources_.renderTarget();
            rt->Clear(D2D1::ColorF(D2D1::ColorF::White));
            onRender(rt);
            device_resources_.endDraw();
        }

        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_SIZE: {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        if (width > 0 && height > 0) {
            device_resources_.resize(width, height);
            auto size = device_resources_.getSize();
            onResize(size.width, size.height);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    case WM_GETMINMAXINFO: {
        if (resizable_) {
            auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            float dpi_scale = device_resources_.dpiX() / 72.0f;
            mmi->ptMinTrackSize.x = static_cast<LONG>(min_size_.width * dpi_scale);
            mmi->ptMinTrackSize.y = static_cast<LONG>(min_size_.height * dpi_scale);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wParam);
        device_resources_.setDpi(static_cast<float>(dpi), static_cast<float>(dpi));

        auto rc = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        auto event = createMouseEvent(lParam, wParam);
        onMouseMove(event);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hwnd_);
        auto event = createMouseEvent(lParam, wParam, MouseButton::Left);
        event.clickCount = 1;
        onMouseDown(event);
        return 0;
    }

    case WM_LBUTTONUP: {
        ReleaseCapture();
        auto event = createMouseEvent(lParam, wParam, MouseButton::Left);
        onMouseUp(event);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        auto event = createMouseEvent(lParam, wParam, MouseButton::Left);
        event.clickCount = 2;
        onMouseDown(event);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        auto event = createMouseEvent(lParam, wParam, MouseButton::Right);
        event.clickCount = 1;
        onMouseDown(event);
        return 0;
    }

    case WM_RBUTTONUP: {
        auto event = createMouseEvent(lParam, wParam, MouseButton::Right);
        onMouseUp(event);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        auto event = createMouseEvent(lParam, wParam);
        event.wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        onMouseWheel(event);
        return 0;
    }

    case WM_KEYDOWN: {
        auto event = createKeyEvent(wParam, lParam);

        // Tab is intercepted at dialog level before child dispatch
        if (event.keyCode == VK_TAB) {
            bool shift = (event.modifiers & Modifiers::Shift) != Modifiers::None;
            advanceFocus(!shift);
            return 0;
        }

        // Dispatch to focused child first
        bool handled = onKeyDown(event);

        // Dialog-level shortcuts for unhandled keys
        if (!handled) {
            if (event.keyCode == VK_ESCAPE) {
                endDialog(IDCANCEL);
            } else if (event.keyCode == VK_RETURN && default_button_) {
                default_button_->onKeyDown(event);
            }
        }
        return 0;
    }

    case WM_KEYUP: {
        auto event = createKeyEvent(wParam, lParam);
        onKeyUp(event);
        return 0;
    }

    case WM_CHAR: {
        KeyEvent event;
        event.character = static_cast<wchar_t>(wParam);
        event.modifiers = getCurrentModifiers();
        onChar(event);
        return 0;
    }

    case WM_CLOSE: {
        if (onClose()) {
            running_ = false;
        }
        return 0;
    }

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void D2DDialog::onRender(ID2D1RenderTarget* rt) {
    renderChildren(rt);
}

void D2DDialog::onResize(float width, float height) {
    bounds_ = Rect{0.0f, 0.0f, width, height};
    updateLayout();
}

void D2DDialog::updateLayout() {
    // Default: measure and arrange children to fill the dialog
    for (auto& child : children_) {
        child->measure(bounds_.size());
        child->arrange(bounds_);
    }
}

void D2DDialog::collectFocusableComponents(D2DContainerComponent* container,
                                            std::vector<D2DUIComponent*>& out) const {
    if (!container) {
        return;
    }

    for (size_t i = 0; i < container->childCount(); ++i) {
        auto* child = container->childAt(i);
        if (!child || !child->isVisible() || !child->isEnabled()) {
            continue;
        }

        // Special handling for TabControl: add itself as tab stop, then content
        if (auto* tab_ctrl = dynamic_cast<D2DTabControl*>(child)) {
            if (tab_ctrl->canReceiveFocus()) {
                out.push_back(tab_ctrl);
            }
            if (auto* content = tab_ctrl->selectedContent()) {
                collectFocusableComponents(content, out);
            }
            continue;
        }

        // Special handling for RadioButton groups: only add the tabbable button
        if (auto* radio = dynamic_cast<D2DRadioButton*>(child)) {
            if (auto* group = radio->group()) {
                auto* tabbable = group->tabbableButton();
                if (tabbable && tabbable == radio) {
                    out.push_back(tabbable);
                }
                continue;
            }
            // Ungrouped radio button: fall through to normal handling
        }

        // If this is a container, recurse into it
        if (auto* sub_container = dynamic_cast<D2DContainerComponent*>(child)) {
            collectFocusableComponents(sub_container, out);
        } else if (child->canReceiveFocus()) {
            out.push_back(child);
        }
    }
}

D2DUIComponent* D2DDialog::findFocusedLeaf() const {
    const D2DContainerComponent* current = this;
    while (current) {
        auto* focused = current->focusedChild();
        if (!focused) {
            return nullptr;
        }

        // Special handling for TabControl
        if (auto* tab_ctrl = dynamic_cast<D2DTabControl*>(focused)) {
            // If TabControl has no focused child (i.e., the tab strip itself is focused)
            if (!tab_ctrl->focusedChild()) {
                return tab_ctrl;
            }
            // TabControl's focused_child_ is the content panel;
            // follow through content to find the actual leaf
            auto* content = tab_ctrl->selectedContent();
            if (content && content->focusedChild()) {
                current = content;
                continue;
            }
            // Content has no focused child: TabControl itself is the focus target
            return tab_ctrl;
        }

        if (auto* container = dynamic_cast<D2DContainerComponent*>(focused)) {
            current = container;
        } else {
            return focused;
        }
    }
    return nullptr;
}

bool D2DDialog::advanceFocus(bool forward) {
    // Collect all focusable components from the entire dialog tree
    std::vector<D2DUIComponent*> focusable;
    collectFocusableComponents(const_cast<D2DDialog*>(this), focusable);

    if (focusable.empty()) {
        return false;
    }

    auto* current = findFocusedLeaf();
    if (!current) {
        // No focus yet: focus the first (or last) component
        auto* target = forward ? focusable.front() : focusable.back();
        if (target->parent()) {
            target->parent()->requestFocus(target);
        }
        invalidate();
        return true;
    }

    // Find current in the list
    auto it = std::find(focusable.begin(), focusable.end(), current);
    if (it == focusable.end()) {
        // Current focused component not in list (might be hidden/disabled)
        auto* target = forward ? focusable.front() : focusable.back();
        if (target->parent()) {
            target->parent()->requestFocus(target);
        }
        invalidate();
        return true;
    }

    // Advance with wrapping
    if (forward) {
        ++it;
        if (it == focusable.end()) {
            it = focusable.begin();
        }
    } else {
        if (it == focusable.begin()) {
            it = focusable.end();
        }
        --it;
    }

    auto* target = *it;
    if (target->parent()) {
        target->parent()->requestFocus(target);
    }
    invalidate();
    return true;
}

Size D2DDialog::measure(const Size& available_size) {
    // Dialog size is determined by initial_size_, not by measuring content
    desired_size_ = initial_size_;
    return desired_size_;
}

MouseEvent D2DDialog::createMouseEvent(LPARAM lParam, WPARAM wParam, MouseButton button) const {
    MouseEvent event;
    event.position.x = static_cast<float>(GET_X_LPARAM(lParam));
    event.position.y = static_cast<float>(GET_Y_LPARAM(lParam));

    POINT screen_pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ClientToScreen(hwnd_, &screen_pt);
    event.screenPosition.x = static_cast<float>(screen_pt.x);
    event.screenPosition.y = static_cast<float>(screen_pt.y);

    event.button = button;
    event.modifiers = getCurrentModifiers();

    return event;
}

KeyEvent D2DDialog::createKeyEvent(WPARAM wParam, LPARAM lParam) const {
    KeyEvent event;
    event.keyCode = static_cast<uint32_t>(wParam);
    event.repeat = (lParam & 0x40000000) != 0;
    event.modifiers = getCurrentModifiers();
    return event;
}

Modifiers D2DDialog::getCurrentModifiers() const {
    Modifiers mods = Modifiers::None;
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        mods = mods | Modifiers::Shift;
    }
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        mods = mods | Modifiers::Ctrl;
    }
    if (GetKeyState(VK_MENU) & 0x8000) {
        mods = mods | Modifiers::Alt;
    }
    return mods;
}

}  // namespace nive::ui::d2d
