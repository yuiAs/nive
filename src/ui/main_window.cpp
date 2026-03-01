/// @file main_window.cpp
/// @brief Main window implementation

#include "main_window.hpp"

#include <CommCtrl.h>
#include <ShlObj.h>
#include <windowsx.h>

#include <array>
#include <format>

#include "app.hpp"
#include "components/directory_tree.hpp"
#include "components/file_list_view.hpp"
#include "components/thumbnail_grid.hpp"
#include "core/fs/file_operations.hpp"
#include "core/i18n/i18n.hpp"
#include "core/thumbnail/thumbnail_request.hpp"
#include "d2d/dialog/settings/d2d_settings_dialog.hpp"
#include "file_operation_manager.hpp"
#ifdef NIVE_DEBUG_D2D_TEST
    #include "d2d/dialog/test/d2d_test_dialog.hpp"
#endif

namespace nive::ui {

namespace {
constexpr wchar_t kWindowClass[] = L"NiveMainWindow";
constexpr wchar_t kAppTitle[] = L"nive";
}  // namespace

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    if (ui_font_) {
        DeleteObject(ui_font_);
    }
    if (menu_) {
        DestroyMenu(menu_);
    }
}

bool MainWindow::create(HINSTANCE hInstance) {
    hinstance_ = hInstance;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // Load settings for window position
    auto& settings = App::instance().settings();
    int x = settings.main_window.x;
    int y = settings.main_window.y;
    int width = settings.main_window.width;
    int height = settings.main_window.height;
    vsplitter_pos_ = settings.main_window.splitter_pos;
    hsplitter_pos_ = settings.main_window.hsplitter_pos;

    // Validate window position
    if (x == CW_USEDEFAULT || width <= 0) {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
        width = 1280;
        height = 720;
    }

    if (hsplitter_pos_ <= 0) {
        hsplitter_pos_ = 200;
    }

    RECT rc = {0, 0, width, height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);  // TRUE for menu

    hwnd_ =
        CreateWindowExW(0, kWindowClass, kAppTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y,
                        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, this);

    return hwnd_ != nullptr;
}

void MainWindow::show(bool maximized) {
    ShowWindow(hwnd_, maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(hwnd_);
}

void MainWindow::saveState(config::Settings& settings) const {
    if (!hwnd_) {
        return;
    }

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd_, &wp)) {
        return;
    }

    settings.main_window.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

    // rcNormalPosition contains the restored window position even when maximized
    RECT rc = wp.rcNormalPosition;

    // Convert window rect to client rect size to match what we use in create()
    // The stored width/height should be client area size, not window size
    RECT client_rect = {0, 0, 0, 0};
    AdjustWindowRect(&client_rect, WS_OVERLAPPEDWINDOW, TRUE);  // TRUE for menu
    int frame_width = client_rect.right - client_rect.left;
    int frame_height = client_rect.bottom - client_rect.top;

    int width = (rc.right - rc.left) - frame_width;
    int height = (rc.bottom - rc.top) - frame_height;

    // Only save if dimensions are valid (non-zero)
    if (width > 0 && height > 0) {
        settings.main_window.x = rc.left;
        settings.main_window.y = rc.top;
        settings.main_window.width = width;
        settings.main_window.height = height;
    }

    settings.main_window.splitter_pos = vsplitter_pos_;
    settings.main_window.hsplitter_pos = hsplitter_pos_;

    // Save file list column widths
    if (file_list_) {
        auto widths = file_list_->getColumnWidths();
        settings.file_list_columns.name = static_cast<float>(widths[0]);
        settings.file_list_columns.size = static_cast<float>(widths[1]);
        settings.file_list_columns.date = static_cast<float>(widths[2]);
        settings.file_list_columns.path = static_cast<float>(widths[3]);
        settings.file_list_columns.dimensions = static_cast<float>(widths[4]);
    }
}

namespace {

// Format byte size into a human-readable string (e.g. "1.23 MB")
std::wstring formatSize(uint64_t bytes) {
    constexpr uint64_t kKB = 1024;
    constexpr uint64_t kMB = kKB * 1024;
    constexpr uint64_t kGB = kMB * 1024;
    constexpr uint64_t kTB = kGB * 1024;

    if (bytes >= kTB) {
        return std::format(L"{:.2f} TB", static_cast<double>(bytes) / kTB);
    } else if (bytes >= kGB) {
        return std::format(L"{:.2f} GB", static_cast<double>(bytes) / kGB);
    } else if (bytes >= kMB) {
        return std::format(L"{:.2f} MB", static_cast<double>(bytes) / kMB);
    } else if (bytes >= kKB) {
        return std::format(L"{:.2f} KB", static_cast<double>(bytes) / kKB);
    } else {
        return std::format(L"{} B", bytes);
    }
}

}  // namespace

void MainWindow::updateStatusBar() {
    if (!status_bar_) {
        return;
    }

    auto& state = App::instance().state();
    auto path = state.currentPath().wstring();
    auto files = state.files();
    auto sel = state.selection();

    // Part 0: Path (or "Ready" if no path set)
    if (path.empty()) {
        auto ready = std::wstring(i18n::tr("status.ready"));
        SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(ready.c_str()));
    } else {
        SendMessageW(status_bar_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(path.c_str()));
    }

    // Part 1: Selection info + file count
    std::wstring info_text;
    if (!sel.empty()) {
        uint64_t total_size = 0;
        for (size_t idx : sel.indices) {
            if (idx < files.size()) {
                total_size += files[idx].size_bytes;
            }
        }
        info_text = std::format(L"{} selected ({})  |  {} files", sel.count(),
                                formatSize(total_size), files.size());
    } else {
        info_text = std::format(L"{} files", files.size());
    }
    SendMessageW(status_bar_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(info_text.c_str()));

    // Part 2: Thumbnail count
    size_t thumb_count = grid_ ? grid_->thumbnailCount() : 0;
    auto thumb_text =
        std::vformat(i18n::tr("status.thumbnail_count"), std::make_wformat_args(thumb_count));
    SendMessageW(status_bar_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(thumb_text.c_str()));
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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

    case WM_DESTROY:
        onDestroy();
        return 0;

    case WM_CLOSE:
        // Save window state before destruction
        saveState(App::instance().settings());
        DestroyWindow(hwnd_);
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (tree_ && nmhdr->hwndFrom == tree_->hwnd()) {
            return tree_->handleNotify(nmhdr);
        } else if (file_list_ && nmhdr->hwndFrom == file_list_->hwnd()) {
            file_list_->handleNotify(nmhdr);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd_, &pt);

        RECT client_rc;
        GetClientRect(hwnd_, &client_rc);
        int right_pane_top = 0;
        int right_pane_bottom = client_rc.bottom - kStatusBarHeight;

        // Check vertical splitter
        if (pt.x >= vsplitter_pos_ && pt.x < vsplitter_pos_ + kSplitterWidth &&
            pt.y >= right_pane_top && pt.y < right_pane_bottom) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return TRUE;
        }

        // Check horizontal splitter (in right pane)
        int hsplitter_y = right_pane_top + hsplitter_pos_;
        if (pt.x > vsplitter_pos_ + kSplitterWidth && pt.y >= hsplitter_y &&
            pt.y < hsplitter_y + kSplitterWidth) {
            SetCursor(LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT client_rc;
        GetClientRect(hwnd_, &client_rc);
        int right_pane_bottom = client_rc.bottom - kStatusBarHeight;

        // Check vertical splitter
        if (x >= vsplitter_pos_ && x < vsplitter_pos_ + kSplitterWidth && y < right_pane_bottom) {
            onVsplitterDragStart(x);
            return 0;
        }

        // Check horizontal splitter
        int hsplitter_y = hsplitter_pos_;
        if (x > vsplitter_pos_ + kSplitterWidth && y >= hsplitter_y &&
            y < hsplitter_y + kSplitterWidth) {
            onHsplitterDragStart(y);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
        if (dragging_vsplitter_) {
            onVsplitterDrag(GET_X_LPARAM(lParam));
            return 0;
        }
        if (dragging_hsplitter_) {
            onHsplitterDrag(GET_Y_LPARAM(lParam));
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (dragging_vsplitter_) {
            onVsplitterDragEnd();
            return 0;
        }
        if (dragging_hsplitter_) {
            onHsplitterDragEnd();
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        dragging_vsplitter_ = false;
        dragging_hsplitter_ = false;
        break;

    case WM_THUMBNAIL_READY:
        App::instance().processThumbnailResults();
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::onCreate() {
    applyFont();
    createMenu();
    createStatusBar();
    createChildControls();
}

void MainWindow::onSize(int width, int height) {
    // Resize status bar
    if (status_bar_) {
        SendMessageW(status_bar_, WM_SIZE, 0, 0);

        // Update status bar parts
        int parts[] = {width / 2, width * 3 / 4, -1};
        SendMessageW(status_bar_, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));
    }

    updateLayout();
}

void MainWindow::onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);

    drawSplitters(hdc);

    EndPaint(hwnd_, &ps);
}

void MainWindow::onDestroy() {
    PostQuitMessage(0);
}

void MainWindow::onCommand(WORD id) {
    auto& settings = App::instance().settings();

    switch (id) {
    case kIdFileSettings:
        if (d2d::showD2DSettingsDialog(hwnd_, settings)) {
            // Settings were changed, save and apply
            App::instance().saveSettings();
            if (tree_) {
                tree_->updateNetworkShares(settings.network_shares);
            }
            App::instance().refresh();
            updateSortMenu();
        }
        break;

    case kIdFileRefresh: {
        HWND focused = GetFocus();
        if (tree_ && focused == tree_->hwnd()) {
            // Refresh only the directory tree (selected node)
            auto selected = tree_->selectedPath();
            if (!selected.empty()) {
                tree_->refreshPath(selected);
            }
        } else {
            // Refresh the right pane (file list / thumbnail grid)
            App::instance().refresh();
        }
        break;
    }

    case kIdFileExit:
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;

    case kIdViewThumbnails:
        // TODO: Switch to thumbnail view mode
        break;

    case kIdViewDetails:
        // TODO: Switch to details view mode
        break;

    // Sort Method
    case kIdSortNatural:
        settings.sort.method = config::SortMethod::Natural;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    case kIdSortLexicographic:
        settings.sort.method = config::SortMethod::Lexicographic;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    case kIdSortDateModified:
        settings.sort.method = config::SortMethod::DateModified;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    case kIdSortDateCreated:
        settings.sort.method = config::SortMethod::DateCreated;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    case kIdSortSize:
        settings.sort.method = config::SortMethod::Size;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    // Sort Order
    case kIdSortAscending:
        settings.sort.ascending = true;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

    case kIdSortDescending:
        settings.sort.ascending = false;
        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
        break;

#ifdef NIVE_DEBUG_D2D_TEST
    case kIdDebugD2DTest:
        d2d::showD2DTestDialog(hwnd_);
        break;
#endif
    }
}

void MainWindow::createMenu() {
    using i18n::tr;
    menu_ = CreateMenu();

    // File menu
    HMENU file_menu = CreatePopupMenu();
    AppendMenuW(file_menu, MF_STRING, kIdFileRefresh, tr("menu.file.refresh").c_str());
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdFileSettings, tr("menu.file.settings").c_str());
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdFileExit, tr("menu.file.exit").c_str());
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu),
                tr("menu.file.label").c_str());

    // View menu
    HMENU view_menu = CreatePopupMenu();
    AppendMenuW(view_menu, MF_STRING, kIdViewThumbnails, tr("menu.view.thumbnails").c_str());
    AppendMenuW(view_menu, MF_STRING, kIdViewDetails, tr("menu.view.details").c_str());
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);

    // Sort Method submenu
    HMENU sort_method_menu = CreatePopupMenu();
    AppendMenuW(sort_method_menu, MF_STRING, kIdSortNatural,
                tr("menu.view.sort_method.natural").c_str());
    AppendMenuW(sort_method_menu, MF_STRING, kIdSortLexicographic,
                tr("menu.view.sort_method.lexicographic").c_str());
    AppendMenuW(sort_method_menu, MF_STRING, kIdSortDateModified,
                tr("menu.view.sort_method.date_modified").c_str());
    AppendMenuW(sort_method_menu, MF_STRING, kIdSortDateCreated,
                tr("menu.view.sort_method.date_created").c_str());
    AppendMenuW(sort_method_menu, MF_STRING, kIdSortSize,
                tr("menu.view.sort_method.size").c_str());
    AppendMenuW(view_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sort_method_menu),
                tr("menu.view.sort_method.label").c_str());

    // Sort Order submenu
    HMENU sort_order_menu = CreatePopupMenu();
    AppendMenuW(sort_order_menu, MF_STRING, kIdSortAscending,
                tr("menu.view.sort_order.ascending").c_str());
    AppendMenuW(sort_order_menu, MF_STRING, kIdSortDescending,
                tr("menu.view.sort_order.descending").c_str());
    AppendMenuW(view_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sort_order_menu),
                tr("menu.view.sort_order.label").c_str());

    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu),
                tr("menu.view.label").c_str());

    // Help menu
    HMENU help_menu = CreatePopupMenu();
#ifdef NIVE_DEBUG_D2D_TEST
    AppendMenuW(help_menu, MF_STRING, kIdDebugD2DTest, tr("menu.help.d2d_test").c_str());
#endif
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu),
                tr("menu.help.label").c_str());

    SetMenu(hwnd_, menu_);
    updateSortMenu();
}

void MainWindow::createStatusBar() {
    status_bar_ = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(kIdStatusBar)), hinstance_, nullptr);

    if (status_bar_ && ui_font_) {
        SendMessageW(status_bar_, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    }

    // Initialize status bar parts
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int parts[] = {rc.right / 2, rc.right * 3 / 4, -1};
    SendMessageW(status_bar_, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

    updateStatusBar();
}

void MainWindow::createChildControls() {
    // Create directory tree
    tree_ = std::make_unique<DirectoryTree>();
    tree_->create(hwnd_, hinstance_, kIdDirectoryTree);

    // Set up archive manager and initialize with network shares
    tree_->setArchiveManager(App::instance().archive());
    tree_->initialize(App::instance().settings().network_shares);

    if (ui_font_) {
        SendMessageW(tree_->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    }

    tree_->onSelectionChanged(
        [](const std::filesystem::path& path) { App::instance().navigateTo(path); });

    // Enable drop target and set up file drop callback
    tree_->enableDropTarget(true);
    tree_->onFileDrop([this](const std::vector<std::filesystem::path>& files,
                             const std::filesystem::path& dest_path, DWORD effect) {
        if (file_op_manager_) {
            auto result = file_op_manager_->handleDrop(files, dest_path, effect);
            if (result.succeeded() || result.partiallySucceeded()) {
                // Refresh if files were moved/copied to current directory
                if (dest_path == App::instance().state().currentPath()) {
                    App::instance().refresh();
                }
            }
        }
    });

    // Create file list view
    file_list_ = std::make_unique<FileListView>();
    file_list_->create(hwnd_, hinstance_, kIdFileList);

    if (ui_font_) {
        SendMessageW(file_list_->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    }

    // Restore column widths from settings
    {
        const auto& cols = App::instance().settings().file_list_columns;
        std::array<int, 5> widths = {
            static_cast<int>(cols.name), static_cast<int>(cols.size),
            static_cast<int>(cols.date), static_cast<int>(cols.path),
            static_cast<int>(cols.dimensions)};
        file_list_->setColumnWidths(widths);
    }

    file_list_->onItemActivated([](size_t index) {
        auto file = App::instance().state().fileAt(index);
        if (file) {
            if (file->is_directory()) {
                App::instance().navigateTo(file->path);
            } else if (file->is_image()) {
                auto vpath = file->virtual_path.value_or(archive::VirtualPath(file->path));
                App::instance().openImage(vpath);
            }
        }
    });

    file_list_->onSelectionChanged([](const std::vector<size_t>& indices) {
        Selection sel;
        sel.indices = indices;
        App::instance().state().setSelection(sel);
    });

    file_list_->onSortChanged([this](FileListColumn column, bool ascending) {
        auto& settings = App::instance().settings();

        // Map column to sort method (Name uses Natural, Date uses DateModified)
        switch (column) {
        case FileListColumn::Name:
            settings.sort.method = config::SortMethod::Natural;
            break;
        case FileListColumn::Size:
            settings.sort.method = config::SortMethod::Size;
            break;
        case FileListColumn::Date:
            settings.sort.method = config::SortMethod::DateModified;
            break;
        case FileListColumn::Resolution:
            // Resolution column doesn't change sort method
            return;
        }
        settings.sort.ascending = ascending;

        App::instance().saveSettings();
        App::instance().refresh();
        updateSortMenu();
    });

    file_list_->onDeleteRequested([this](const std::vector<std::filesystem::path>& files) {
        if (file_op_manager_) {
            file_op_manager_->deleteFiles(files);
        }
    });

    // Create thumbnail grid
    grid_ = std::make_unique<ThumbnailGrid>();
    grid_->create(hwnd_, hinstance_, kIdThumbnailGrid);

    if (ui_font_) {
        SendMessageW(grid_->hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    }

    auto& settings = App::instance().settings();
    grid_->setThumbnailSize(settings.thumbnails.display_size);

    grid_->onItemActivated([](size_t index) {
        auto file = App::instance().state().fileAt(index);
        if (file) {
            if (file->is_directory()) {
                App::instance().navigateTo(file->path);
            } else if (file->is_image()) {
                auto vpath = file->virtual_path.value_or(archive::VirtualPath(file->path));
                App::instance().openImage(vpath);
            }
        }
    });

    grid_->onSelectionChanged([](const std::vector<size_t>& indices) {
        Selection sel;
        sel.indices = indices;
        App::instance().state().setSelection(sel);
    });

    grid_->onThumbnailRequest([](const archive::VirtualPath& path) {
        App::instance().requestThumbnail(path, thumbnail::Priority::High);
    });

    grid_->onThumbnailCancel([]() { App::instance().thumbnails()->cancelAll(); });

    grid_->onDeleteRequested([this](const std::vector<std::filesystem::path>& files) {
        if (file_op_manager_) {
            file_op_manager_->deleteFiles(files);
        }
    });

    grid_->onRenameRequested([this](size_t index, const std::wstring& new_name) {
        auto file = App::instance().state().fileAt(index);
        if (!file) {
            return;
        }
        auto result = fs::renameFile(file->path, new_name);
        if (result) {
            App::instance().refresh();
        } else {
            auto msg = std::wstring(L"Failed to rename: ") +
                       std::wstring(to_string(result.error()).begin(),
                                    to_string(result.error()).end());
            MessageBoxW(hwnd_, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        }
    });

    // Create file operation manager
    file_op_manager_ = std::make_unique<FileOperationManager>(hwnd_);

    // Set up completion callback for file operations
    file_op_manager_->onOperationComplete([](bool success, const fs::FileOperationResult& result) {
        if (success) {
            // Refresh the current directory view
            App::instance().refresh();
        }
    });

    // Listen for state changes
    App::instance().state().onChange([this](AppState::ChangeType type) {
        switch (type) {
        case AppState::ChangeType::CurrentPath:
            directory_changed_ = true;
            if (tree_) {
                // Only call selectPath() when navigation originated from outside
                // the tree (e.g. file list, address bar). When the user clicks a
                // tree item, the TreeView already has the correct selection and
                // calling selectPath() would unnecessarily expand the node.
                auto target = App::instance().state().currentPath();
                if (tree_->selectedPath() != target) {
                    tree_->selectPath(target);
                }
            }
            updateStatusBar();
            break;

        case AppState::ChangeType::DirectoryContents: {
            bool preserve_scroll = !directory_changed_;
            directory_changed_ = false;
            if (file_list_) {
                file_list_->setItems(App::instance().state().files());
            }
            if (grid_) {
                grid_->setItems(App::instance().state().files(), preserve_scroll);
            }
            updateStatusBar();
            break;
        }

        case AppState::ChangeType::Selection:
            updateStatusBar();
            break;

        default:
            break;
        }
    });

    updateLayout();
}

void MainWindow::updateLayout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Account for status bar
    int content_height = height - kStatusBarHeight;

    // Clamp vertical splitter position
    vsplitter_pos_ = (std::max)(kMinPaneWidth,
                                (std::min)(vsplitter_pos_, width - kMinPaneWidth - kSplitterWidth));

    // Right pane dimensions
    int right_x = vsplitter_pos_ + kSplitterWidth;
    int right_width = width - right_x;

    // Clamp horizontal splitter position
    hsplitter_pos_ =
        (std::max)(kMinPaneHeight,
                   (std::min)(hsplitter_pos_, content_height - kMinPaneHeight - kSplitterWidth));

    // Position directory tree (full height of content area)
    if (tree_) {
        tree_->setBounds(0, 0, vsplitter_pos_, content_height);
    }

    // Position file list (top of right pane)
    if (file_list_) {
        file_list_->setBounds(right_x, 0, right_width, hsplitter_pos_);
    }

    // Position thumbnail grid (bottom of right pane)
    if (grid_) {
        int grid_y = hsplitter_pos_ + kSplitterWidth;
        int grid_height = content_height - grid_y;
        grid_->setBounds(right_x, grid_y, right_width, grid_height);
    }

    // Invalidate splitter areas
    RECT vsplitter_rc = {vsplitter_pos_, 0, vsplitter_pos_ + kSplitterWidth, content_height};
    InvalidateRect(hwnd_, &vsplitter_rc, TRUE);

    RECT hsplitter_rc = {right_x, hsplitter_pos_, width, hsplitter_pos_ + kSplitterWidth};
    InvalidateRect(hwnd_, &hsplitter_rc, TRUE);
}

void MainWindow::applyFont() {
    // Create system UI font (Segoe UI)
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        ui_font_ = CreateFontIndirectW(&ncm.lfMessageFont);
    }

    if (!ui_font_) {
        // Fallback: Create Segoe UI font manually
        ui_font_ = CreateFontW(-12,                          // Height
                               0,                            // Width
                               0,                            // Escapement
                               0,                            // Orientation
                               FW_NORMAL,                    // Weight
                               FALSE,                        // Italic
                               FALSE,                        // Underline
                               FALSE,                        // StrikeOut
                               DEFAULT_CHARSET,              // Charset
                               OUT_DEFAULT_PRECIS,           // Output precision
                               CLIP_DEFAULT_PRECIS,          // Clip precision
                               CLEARTYPE_QUALITY,            // Quality
                               DEFAULT_PITCH | FF_DONTCARE,  // Pitch and family
                               L"Segoe UI"                   // Font name
        );
    }
}

void MainWindow::onVsplitterDragStart(int x) {
    dragging_vsplitter_ = true;
    vdrag_start_x_ = x;
    vdrag_start_pos_ = vsplitter_pos_;
    SetCapture(hwnd_);
}

void MainWindow::onVsplitterDrag(int x) {
    int delta = x - vdrag_start_x_;
    vsplitter_pos_ = vdrag_start_pos_ + delta;
    updateLayout();
}

void MainWindow::onVsplitterDragEnd() {
    dragging_vsplitter_ = false;
    ReleaseCapture();
}

void MainWindow::onHsplitterDragStart(int y) {
    dragging_hsplitter_ = true;
    hdrag_start_y_ = y;
    hdrag_start_pos_ = hsplitter_pos_;
    SetCapture(hwnd_);
}

void MainWindow::onHsplitterDrag(int y) {
    int delta = y - hdrag_start_y_;
    hsplitter_pos_ = hdrag_start_pos_ + delta;
    updateLayout();
}

void MainWindow::onHsplitterDragEnd() {
    dragging_hsplitter_ = false;
    ReleaseCapture();
}

void MainWindow::drawSplitters(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int content_height = rc.bottom - kStatusBarHeight;
    int right_x = vsplitter_pos_ + kSplitterWidth;

    // Draw vertical splitter
    RECT vsplitter_rc = {vsplitter_pos_, 0, vsplitter_pos_ + kSplitterWidth, content_height};
    FillRect(hdc, &vsplitter_rc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));

    // Draw horizontal splitter (in right pane)
    RECT hsplitter_rc = {right_x, hsplitter_pos_, rc.right, hsplitter_pos_ + kSplitterWidth};
    FillRect(hdc, &hsplitter_rc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));

    // Draw edges for vertical splitter
    HPEN light_pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
    HPEN dark_pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));

    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, light_pen));

    // Vertical splitter edges
    MoveToEx(hdc, vsplitter_rc.left, vsplitter_rc.top, nullptr);
    LineTo(hdc, vsplitter_rc.left, vsplitter_rc.bottom);

    SelectObject(hdc, dark_pen);
    MoveToEx(hdc, vsplitter_rc.right - 1, vsplitter_rc.top, nullptr);
    LineTo(hdc, vsplitter_rc.right - 1, vsplitter_rc.bottom);

    // Horizontal splitter edges
    SelectObject(hdc, light_pen);
    MoveToEx(hdc, hsplitter_rc.left, hsplitter_rc.top, nullptr);
    LineTo(hdc, hsplitter_rc.right, hsplitter_rc.top);

    SelectObject(hdc, dark_pen);
    MoveToEx(hdc, hsplitter_rc.left, hsplitter_rc.bottom - 1, nullptr);
    LineTo(hdc, hsplitter_rc.right, hsplitter_rc.bottom - 1);

    SelectObject(hdc, old_pen);
    DeleteObject(light_pen);
    DeleteObject(dark_pen);
}

void MainWindow::updateSortMenu() {
    if (!menu_) {
        return;
    }

    const auto& settings = App::instance().settings();

    // Update Sort Method checkmarks
    CheckMenuItem(
        menu_, kIdSortNatural,
        MF_BYCOMMAND |
            (settings.sort.method == config::SortMethod::Natural ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu_, kIdSortLexicographic,
                  MF_BYCOMMAND |
                      (settings.sort.method == config::SortMethod::Lexicographic ? MF_CHECKED
                                                                                 : MF_UNCHECKED));
    CheckMenuItem(
        menu_, kIdSortDateModified,
        MF_BYCOMMAND |
            (settings.sort.method == config::SortMethod::DateModified ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu_, kIdSortDateCreated,
        MF_BYCOMMAND |
            (settings.sort.method == config::SortMethod::DateCreated ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu_, kIdSortSize,
                  MF_BYCOMMAND | (settings.sort.method == config::SortMethod::Size ? MF_CHECKED
                                                                                   : MF_UNCHECKED));

    // Update Sort Order checkmarks
    CheckMenuItem(menu_, kIdSortAscending,
                  MF_BYCOMMAND | (settings.sort.ascending ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu_, kIdSortDescending,
                  MF_BYCOMMAND | (!settings.sort.ascending ? MF_CHECKED : MF_UNCHECKED));
}

}  // namespace nive::ui
