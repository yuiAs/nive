/// @file image_viewer_window.cpp
/// @brief Image viewer window implementation with Direct2D rendering

#include "image_viewer_window.hpp"

#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <format>
#include <fstream>

#include "app.hpp"
#include "core/i18n/i18n.hpp"
#include "core/image/wic_decoder.hpp"
#include "d2d/core/bitmap_utils.hpp"
#include "file_operation_manager.hpp"

namespace nive::ui {

namespace {
constexpr wchar_t kWindowClass[] = L"NiveImageViewerWindow";
}  // namespace

ImageViewerWindow::ImageViewerWindow() = default;

ImageViewerWindow::~ImageViewerWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
    if (menu_) {
        DestroyMenu(menu_);
    }
}

bool ImageViewerWindow::create(HINSTANCE hInstance) {
    hinstance_ = hInstance;

    // Register window class if not already registered
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);

    if (!GetClassInfoExW(hInstance, kWindowClass, &wc)) {
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // D2D handles painting
        wc.lpszClassName = kWindowClass;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc)) {
            return false;
        }
    }

    // Load settings for window position
    auto& settings = App::instance().settings();
    int x = settings.viewer_window.x;
    int y = settings.viewer_window.y;
    int width = settings.viewer_window.width;
    int height = settings.viewer_window.height;

    // Validate window position
    if (x == 0 && y == 0 && width == 0 && height == 0) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        width = 800;
        height = 600;
    }

    // Ensure minimum size
    if (width < 400)
        width = 400;
    if (height < 300)
        height = 300;

    hwnd_ = CreateWindowExW(
        0, kWindowClass, i18n::tr("viewer.default_title").c_str(), WS_OVERLAPPEDWINDOW,
        x == CW_USEDEFAULT ? CW_USEDEFAULT : x,
        y == CW_USEDEFAULT ? CW_USEDEFAULT : y, width, height, nullptr, nullptr, hInstance, this);

    if (!hwnd_) {
        return false;
    }

    // Initialize D2D render target
    if (!device_resources_.setTargetWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    last_resource_epoch_ = device_resources_.resourceEpoch();

    return true;
}

void ImageViewerWindow::show() {
    // Recreate window if it was closed
    if (!hwnd_ && hinstance_) {
        create(hinstance_);
        // Recreate D2D bitmap from existing image since setImage() may have
        // been called before window recreation (when render target was absent)
        recreateBitmap();
        updateTitle();
    }

    if (hwnd_) {
        auto& settings = App::instance().settings();
        ShowWindow(hwnd_, settings.viewer_window.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
        SetForegroundWindow(hwnd_);
        UpdateWindow(hwnd_);
    }
}

void ImageViewerWindow::close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool ImageViewerWindow::isVisible() const noexcept {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void ImageViewerWindow::setImage(const archive::VirtualPath& path) {
    current_path_ = path;
    image_.reset();
    bitmap_.Reset();

    if (path.empty()) {
        updateTitle();
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return;
    }

    // Decode the image: try plugins first, then fall back to WIC
    auto* plugin_mgr = App::instance().plugins();

    std::expected<image::DecodedImage, image::DecodeError> result;

    if (path.is_in_archive()) {
        // Load from archive
        auto* archive_mgr = App::instance().archive();
        if (!archive_mgr) {
            return;
        }

        auto data = archive_mgr->extractToMemory(path);
        if (!data) {
            return;
        }

        // Try plugin decode first
        if (plugin_mgr) {
            std::string ext = std::filesystem::path(path.filename()).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            });
            auto plugin_result = plugin_mgr->decode(data->data(), data->size(), ext);
            if (plugin_result) {
                result = std::move(*plugin_result);
            }
        }

        // Fallback to WIC
        if (!result) {
            image::WicDecoder decoder;
            if (decoder.isAvailable()) {
                result = decoder.decodeFromMemory(*data);
            }
        }
    } else {
        std::string ext = path.archive_path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        });

        // Try plugin decode first (read file into memory)
        if (plugin_mgr && plugin_mgr->supportsExtension(ext)) {
            std::ifstream file(path.archive_path(), std::ios::binary | std::ios::ate);
            if (file) {
                auto file_size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::vector<uint8_t> data(static_cast<size_t>(file_size));
                if (file.read(reinterpret_cast<char*>(data.data()), file_size)) {
                    auto plugin_result = plugin_mgr->decode(data.data(), data.size(), ext);
                    if (plugin_result) {
                        result = std::move(*plugin_result);
                    }
                }
            }
        }

        // Fallback to WIC
        if (!result) {
            image::WicDecoder decoder;
            if (decoder.isAvailable()) {
                result = decoder.decode(path.archive_path());
            }
        }
    }

    if (result) {
        image_ = std::make_unique<image::DecodedImage>(std::move(*result));

        // Create D2D bitmap from decoded image
        if (device_resources_.isValid()) {
            bitmap_ = d2d::createBitmapFromDecodedImage(device_resources_.renderTarget(), *image_);
        }

        // Reset zoom/scroll based on display mode
        if (display_mode_ == config::ViewerDisplayMode::Original) {
            zoom_ = 1.0f;
            centerImage();
        } else {
            scroll_x_ = 0;
            scroll_y_ = 0;
        }
    }

    updateTitle();
    updateStatusBar();

    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImageViewerWindow::setDisplayMode(config::ViewerDisplayMode mode) {
    if (display_mode_ != mode) {
        display_mode_ = mode;

        if (mode == config::ViewerDisplayMode::Original) {
            zoom_ = 1.0f;
            centerImage();
        } else {
            scroll_x_ = 0;
            scroll_y_ = 0;
        }

        updateTitle();
        updateMenuCheck();

        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void ImageViewerWindow::zoomIn() {
    if (display_mode_ != config::ViewerDisplayMode::Original) {
        display_mode_ = config::ViewerDisplayMode::Original;
        updateMenuCheck();
    }

    float new_zoom = zoom_ * kZoomStep;
    if (new_zoom <= kMaxZoom) {
        zoom_ = new_zoom;
        clampScroll();
        updateTitle();

        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void ImageViewerWindow::zoomOut() {
    if (display_mode_ != config::ViewerDisplayMode::Original) {
        display_mode_ = config::ViewerDisplayMode::Original;
        updateMenuCheck();
    }

    float new_zoom = zoom_ / kZoomStep;
    if (new_zoom >= kMinZoom) {
        zoom_ = new_zoom;
        clampScroll();
        updateTitle();

        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void ImageViewerWindow::zoomReset() {
    display_mode_ = config::ViewerDisplayMode::Original;
    zoom_ = 1.0f;
    centerImage();
    updateTitle();
    updateMenuCheck();

    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImageViewerWindow::fitToWindow() {
    display_mode_ = config::ViewerDisplayMode::FitToWindow;
    scroll_x_ = 0;
    scroll_y_ = 0;
    updateTitle();
    updateMenuCheck();

    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImageViewerWindow::nextImage() {
    auto& state = App::instance().state();
    if (state.viewerNext()) {
        auto path = state.viewerImage();
        if (path) {
            setImage(*path);
        }
    }
}

void ImageViewerWindow::previousImage() {
    auto& state = App::instance().state();
    if (state.viewerPrevious()) {
        auto path = state.viewerImage();
        if (path) {
            setImage(*path);
        }
    }
}

void ImageViewerWindow::saveState(config::Settings& settings) const {
    if (!hwnd_) {
        return;
    }

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd_, &wp)) {
        return;
    }

    settings.viewer_window.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

    // rcNormalPosition contains the restored window position even when maximized
    RECT rc = wp.rcNormalPosition;
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Only save if dimensions are valid
    if (width > 0 && height > 0) {
        settings.viewer_window.x = rc.left;
        settings.viewer_window.y = rc.top;
        settings.viewer_window.width = width;
        settings.viewer_window.height = height;
    }

    settings.viewer_display_mode = display_mode_;
}

LRESULT CALLBACK ImageViewerWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ImageViewerWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ImageViewerWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<ImageViewerWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT ImageViewerWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        onPaint();
        return 0;

    case WM_ERASEBKGND:
        return 1;  // D2D handles painting

    case WM_DESTROY:
        onDestroy();
        return 0;

    case WM_KEYDOWN:
        onKeyDown(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDOWN:
        onLbuttonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        onLbuttonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        onLbuttonDblclk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && dragging_) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return TRUE;
        }
        break;

    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wParam);
        device_resources_.setDpi(static_cast<float>(dpi), static_cast<float>(dpi));
        recreateBitmap();
        last_resource_epoch_ = device_resources_.resourceEpoch();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void ImageViewerWindow::onCreate() {
    createMenu();
    createStatusBar();
}

void ImageViewerWindow::onSize(int width, int height) {
    if (status_bar_) {
        SendMessageW(status_bar_, WM_SIZE, 0, 0);
    }
    if (width > 0 && height > 0) {
        device_resources_.resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
    clampScroll();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ImageViewerWindow::onPaint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);
    render();
    EndPaint(hwnd_, &ps);
}

void ImageViewerWindow::onDestroy() {
    // Save state before destruction
    saveState(App::instance().settings());
    App::instance().saveSettings();

    // Clear viewer state
    App::instance().state().clearViewerImage();

    // Release D2D resources
    bitmap_.Reset();
    device_resources_.discardResources();

    hwnd_ = nullptr;
}

void ImageViewerWindow::onKeyDown(WPARAM vk) {
    switch (vk) {
    case VK_ESCAPE:
        close();
        break;

    case VK_LEFT:
        previousImage();
        break;

    case VK_RIGHT:
        nextImage();
        break;

    case VK_OEM_PLUS:
    case VK_ADD:
        zoomIn();
        break;

    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        zoomOut();
        break;

    case '0':
    case VK_NUMPAD0:
        zoomReset();
        break;

    case 'F':
        fitToWindow();
        break;

    case '1':
        zoomReset();
        break;

    case VK_DELETE:
        deleteCurrentImage();
        break;

    default:
        break;
    }
}

void ImageViewerWindow::onMouseWheel(int delta, int x, int y) {
    if (!image_ || !image_->valid()) {
        return;
    }

    // Only zoom when Ctrl is pressed
    if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
        return;
    }

    // Convert screen coordinates to client coordinates
    POINT pt = {x, y};
    ScreenToClient(hwnd_, &pt);

    auto client = getImageAreaRect();

    // Only zoom if mouse is in image area
    if (!PtInRect(&client, pt)) {
        return;
    }

    // Switch to Original mode if not already
    if (display_mode_ != config::ViewerDisplayMode::Original) {
        // Calculate current fit zoom before switching
        int view_w = client.right - client.left;
        int view_h = client.bottom - client.top;
        zoom_ = calculateFitZoom(image_->width(), image_->height(), view_w, view_h);
        display_mode_ = config::ViewerDisplayMode::Original;
        scroll_x_ = 0;
        scroll_y_ = 0;
        updateMenuCheck();
    }

    // Calculate zoom centered on mouse position
    float old_zoom = zoom_;
    if (delta > 0) {
        zoom_ = std::min(zoom_ * kZoomStep, kMaxZoom);
    } else {
        zoom_ = std::max(zoom_ / kZoomStep, kMinZoom);
    }

    if (zoom_ != old_zoom) {
        // Adjust scroll to keep point under cursor stationary
        float img_x = (pt.x + scroll_x_) / old_zoom;
        float img_y = (pt.y + scroll_y_) / old_zoom;

        scroll_x_ = static_cast<int>(img_x * zoom_ - pt.x);
        scroll_y_ = static_cast<int>(img_y * zoom_ - pt.y);

        clampScroll();
        updateTitle();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImageViewerWindow::onLbuttonDown(int x, int y) {
    if (!image_ || !image_->valid()) {
        return;
    }

    if (display_mode_ == config::ViewerDisplayMode::Original) {
        dragging_ = true;
        drag_start_x_ = x;
        drag_start_y_ = y;
        drag_start_scroll_x_ = scroll_x_;
        drag_start_scroll_y_ = scroll_y_;
        SetCapture(hwnd_);
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
    }
}

void ImageViewerWindow::onLbuttonUp(int x, int y) {
    (void)x;
    (void)y;

    if (dragging_) {
        dragging_ = false;
        ReleaseCapture();
    }
}

void ImageViewerWindow::onMouseMove(int x, int y) {
    if (dragging_ && image_ && image_->valid()) {
        int dx = x - drag_start_x_;
        int dy = y - drag_start_y_;

        scroll_x_ = drag_start_scroll_x_ - dx;
        scroll_y_ = drag_start_scroll_y_ - dy;

        clampScroll();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImageViewerWindow::onLbuttonDblclk(int x, int y) {
    (void)x;
    (void)y;

    // Toggle between Fit and Original modes
    if (display_mode_ == config::ViewerDisplayMode::Original) {
        fitToWindow();
    } else {
        zoomReset();
    }
}

void ImageViewerWindow::render() {
    // Detect device lost via epoch counter
    uint32_t epoch = device_resources_.resourceEpoch();
    if (epoch != last_resource_epoch_) {
        recreateBitmap();
        last_resource_epoch_ = epoch;
    }

    if (!device_resources_.beginDraw()) {
        return;
    }

    auto* rt = device_resources_.renderTarget();

    // Clear background to black
    rt->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    if (bitmap_ && image_ && image_->valid()) {
        auto image_rect = getImageAreaRect();
        float view_w = static_cast<float>(image_rect.right - image_rect.left);
        float view_h = static_cast<float>(image_rect.bottom - image_rect.top);
        float img_w = static_cast<float>(image_->width());
        float img_h = static_cast<float>(image_->height());

        float display_w, display_h;

        if (display_mode_ == config::ViewerDisplayMode::Original) {
            display_w = img_w * zoom_;
            display_h = img_h * zoom_;
        } else {
            float fit_zoom = calculateFitZoom(
                image_->width(), image_->height(),
                static_cast<int>(view_w), static_cast<int>(view_h));
            display_w = img_w * fit_zoom;
            display_h = img_h * fit_zoom;
        }

        // Calculate position (centered if smaller than view)
        float x = 0.0f, y = 0.0f;

        if (display_mode_ == config::ViewerDisplayMode::Original) {
            if (display_w < view_w) {
                x = (view_w - display_w) / 2.0f;
            } else {
                x = static_cast<float>(-scroll_x_);
            }

            if (display_h < view_h) {
                y = (view_h - display_h) / 2.0f;
            } else {
                y = static_cast<float>(-scroll_y_);
            }
        } else {
            // Centered for fit modes
            x = (view_w - display_w) / 2.0f;
            y = (view_h - display_h) / 2.0f;
        }

        D2D1_RECT_F dest = D2D1::RectF(x, y, x + display_w, y + display_h);
        auto mode = D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
        rt->DrawBitmap(bitmap_.Get(), dest, 1.0f, mode);
    }

    device_resources_.endDraw();
}

void ImageViewerWindow::recreateBitmap() {
    bitmap_.Reset();
    if (image_ && image_->valid() && device_resources_.isValid()) {
        bitmap_ = d2d::createBitmapFromDecodedImage(device_resources_.renderTarget(), *image_);
    }
}

void ImageViewerWindow::updateTitle() {
    if (!hwnd_) {
        return;
    }

    std::wstring title;

    if (image_ && image_->valid()) {
        std::wstring filename = current_path_.filename();
        int w = static_cast<int>(image_->width());
        int h = static_cast<int>(image_->height());

        int zoom_percent;
        if (display_mode_ == config::ViewerDisplayMode::Original) {
            zoom_percent = static_cast<int>(zoom_ * 100 + 0.5f);
        } else {
            auto client = getImageAreaRect();
            int view_w = client.right - client.left;
            int view_h = client.bottom - client.top;
            float fit_zoom = calculateFitZoom(w, h, view_w, view_h);
            zoom_percent = static_cast<int>(fit_zoom * 100 + 0.5f);
        }

        // Calculate current index and total image count
        size_t current_index = 0;
        size_t total_images = 0;
        auto files = App::instance().state().files();
        for (size_t i = 0; i < files.size(); ++i) {
            if (files[i].is_image()) {
                ++total_images;
                if (files[i].name == filename) {
                    current_index = total_images;
                }
            }
        }

        title = std::vformat(i18n::tr("viewer.title_format"),
                             std::make_wformat_args(filename, w, h, zoom_percent, current_index,
                                                    total_images));
    } else {
        title = std::wstring(i18n::tr("viewer.default_title"));
    }

    SetWindowTextW(hwnd_, title.c_str());
}

void ImageViewerWindow::clampScroll() {
    if (!hwnd_ || !image_ || !image_->valid()) {
        return;
    }

    if (display_mode_ != config::ViewerDisplayMode::Original) {
        scroll_x_ = 0;
        scroll_y_ = 0;
        return;
    }

    auto client = getImageAreaRect();
    int view_w = client.right - client.left;
    int view_h = client.bottom - client.top;

    int img_w = static_cast<int>(image_->width() * zoom_);
    int img_h = static_cast<int>(image_->height() * zoom_);

    // If image is smaller than view, no scrolling
    if (img_w <= view_w) {
        scroll_x_ = 0;
    } else {
        int max_scroll = img_w - view_w;
        scroll_x_ = std::clamp(scroll_x_, 0, max_scroll);
    }

    if (img_h <= view_h) {
        scroll_y_ = 0;
    } else {
        int max_scroll = img_h - view_h;
        scroll_y_ = std::clamp(scroll_y_, 0, max_scroll);
    }
}

void ImageViewerWindow::centerImage() {
    scroll_x_ = 0;
    scroll_y_ = 0;

    if (!hwnd_ || !image_ || !image_->valid()) {
        return;
    }

    if (display_mode_ != config::ViewerDisplayMode::Original) {
        return;
    }

    auto client = getImageAreaRect();
    int view_w = client.right - client.left;
    int view_h = client.bottom - client.top;

    int img_w = static_cast<int>(image_->width() * zoom_);
    int img_h = static_cast<int>(image_->height() * zoom_);

    // Center if image is larger than view
    if (img_w > view_w) {
        scroll_x_ = (img_w - view_w) / 2;
    }
    if (img_h > view_h) {
        scroll_y_ = (img_h - view_h) / 2;
    }
}

float ImageViewerWindow::calculateFitZoom(int image_width, int image_height, int view_width,
                                          int view_height) const {
    if (image_width <= 0 || image_height <= 0 || view_width <= 0 || view_height <= 0) {
        return 1.0f;
    }

    float scale_x = static_cast<float>(view_width) / image_width;
    float scale_y = static_cast<float>(view_height) / image_height;
    float scale = std::min(scale_x, scale_y);

    switch (display_mode_) {
    case config::ViewerDisplayMode::Original:
        return zoom_;

    case config::ViewerDisplayMode::FitToWindow:
        return scale;

    case config::ViewerDisplayMode::ShrinkToFit:
        return std::min(scale, 1.0f);

    default:
        return 1.0f;
    }
}

void ImageViewerWindow::createMenu() {
    menu_ = CreateMenu();

    using i18n::tr;

    // File menu
    HMENU file_menu = CreatePopupMenu();
    AppendMenuW(file_menu, MF_STRING, kIdFileExit, tr("viewer.menu.file.exit").c_str());
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu),
                tr("viewer.menu.file.label").c_str());

    // View menu
    HMENU view_menu = CreatePopupMenu();

    // Display Mode submenu
    HMENU display_mode_menu = CreatePopupMenu();
    AppendMenuW(display_mode_menu, MF_STRING, kIdViewOriginal,
                tr("viewer.menu.view.original").c_str());
    AppendMenuW(display_mode_menu, MF_STRING, kIdViewFitToWindow,
                tr("viewer.menu.view.fit_to_window").c_str());
    AppendMenuW(display_mode_menu, MF_STRING, kIdViewShrinkToFit,
                tr("viewer.menu.view.shrink_to_fit").c_str());
    AppendMenuW(view_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(display_mode_menu),
                tr("viewer.menu.view.display_mode").c_str());

    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING, kIdViewZoomIn, tr("viewer.menu.view.zoom_in").c_str());
    AppendMenuW(view_menu, MF_STRING, kIdViewZoomOut, tr("viewer.menu.view.zoom_out").c_str());
    AppendMenuW(view_menu, MF_STRING, kIdViewResetZoom,
                tr("viewer.menu.view.reset_zoom").c_str());

    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu),
                tr("viewer.menu.view.label").c_str());

    SetMenu(hwnd_, menu_);
    updateMenuCheck();
}

void ImageViewerWindow::onCommand(WORD id) {
    switch (id) {
    case kIdFileExit:
        close();
        break;

    case kIdViewOriginal:
        setDisplayMode(config::ViewerDisplayMode::Original);
        break;

    case kIdViewFitToWindow:
        setDisplayMode(config::ViewerDisplayMode::FitToWindow);
        break;

    case kIdViewShrinkToFit:
        setDisplayMode(config::ViewerDisplayMode::ShrinkToFit);
        break;

    case kIdViewZoomIn:
        zoomIn();
        break;

    case kIdViewZoomOut:
        zoomOut();
        break;

    case kIdViewResetZoom:
        zoomReset();
        break;

    default:
        break;
    }
}

void ImageViewerWindow::deleteCurrentImage() {
    if (current_path_.empty() || current_path_.is_in_archive()) {
        return;
    }

    auto file_path = current_path_.archive_path();

    // Determine next image to show before deletion
    auto files = App::instance().state().files();
    std::wstring filename = current_path_.filename();
    std::optional<size_t> current_idx;

    for (size_t i = 0; i < files.size(); ++i) {
        if (files[i].name == filename) {
            current_idx = i;
            break;
        }
    }

    std::optional<archive::VirtualPath> next_image;
    if (current_idx) {
        // Look forward for next image
        for (size_t i = *current_idx + 1; i < files.size(); ++i) {
            if (files[i].is_image()) {
                next_image =
                    files[i].virtual_path.value_or(archive::VirtualPath(files[i].path));
                break;
            }
        }
        // If no next, look backward for previous image
        if (!next_image) {
            for (size_t i = *current_idx; i > 0; --i) {
                if (files[i - 1].is_image()) {
                    next_image =
                        files[i - 1].virtual_path.value_or(archive::VirtualPath(files[i - 1].path));
                    break;
                }
            }
        }
    }

    // Delete with confirmation dialog
    FileOperationManager file_op(hwnd_);
    auto result = file_op.deleteFiles({file_path});

    if (!result.succeeded() && !result.partiallySucceeded()) {
        return;
    }

    // Refresh directory listing
    App::instance().refresh();

    if (next_image) {
        App::instance().state().setViewerImage(*next_image);
        setImage(*next_image);
    } else {
        // No images remaining
        close();
    }
}

void ImageViewerWindow::updateMenuCheck() {
    if (!menu_) {
        return;
    }

    // Update display mode check marks
    CheckMenuItem(
        menu_, kIdViewOriginal,
        MF_BYCOMMAND |
            (display_mode_ == config::ViewerDisplayMode::Original ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu_, kIdViewFitToWindow,
        MF_BYCOMMAND |
            (display_mode_ == config::ViewerDisplayMode::FitToWindow ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu_, kIdViewShrinkToFit,
        MF_BYCOMMAND |
            (display_mode_ == config::ViewerDisplayMode::ShrinkToFit ? MF_CHECKED : MF_UNCHECKED));
}

void ImageViewerWindow::createStatusBar() {
    status_bar_ = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(kIdStatusBar)), hinstance_, nullptr);

    if (!status_bar_) {
        return;
    }

    // Single part spanning the full width
    int parts[] = {-1};
    SendMessageW(status_bar_, SB_SETPARTS, 1, reinterpret_cast<LPARAM>(parts));
}

void ImageViewerWindow::updateStatusBar() {
    if (!status_bar_) {
        return;
    }

    if (current_path_.empty()) {
        SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L""));
        return;
    }

    // Look up file metadata from current directory listing
    std::wstring filename = current_path_.filename();
    auto files = App::instance().state().files();

    for (const auto& file : files) {
        if (file.name == filename) {
            // Format modified time (same format as FileListView: %Y-%m-%d %H:%M)
            auto tt = std::chrono::system_clock::to_time_t(file.modified_time);
            std::tm tm_buf;
            localtime_s(&tm_buf, &tt);

            wchar_t date_buf[64];
            std::wcsftime(date_buf, sizeof(date_buf) / sizeof(wchar_t), L"%Y-%m-%d %H:%M",
                          &tm_buf);

            auto text =
                std::vformat(i18n::tr("viewer.status.modified_date"),
                             std::make_wformat_args(date_buf));
            SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
            return;
        }
    }

    // File not found in listing
    SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L""));
}

RECT ImageViewerWindow::getImageAreaRect() const {
    RECT rc = {};
    if (hwnd_) {
        GetClientRect(hwnd_, &rc);
        if (status_bar_) {
            RECT sb_rc;
            GetWindowRect(status_bar_, &sb_rc);
            int sb_height = sb_rc.bottom - sb_rc.top;
            rc.bottom -= sb_height;
            if (rc.bottom < 0) {
                rc.bottom = 0;
            }
        }
    }
    return rc;
}

}  // namespace nive::ui
