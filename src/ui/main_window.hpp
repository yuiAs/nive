/// @file main_window.hpp
/// @brief Main application window

#pragma once

#include <Windows.h>

#include <memory>
#include <string>

#include "core/config/settings.hpp"

namespace nive::ui {

// Forward declarations
class DirectoryTree;
class FileListView;
class ThumbnailGrid;
class FileOperationManager;

/// @brief Main application window
///
/// Layout:
/// +--------------------------------------------------+
/// |  File  View                            (Menu)    |
/// +--------------------------------------------------+
/// |          |                                       |
/// | Directory|     File List View (ListView)         |
/// |   Tree   |  Name | Size | Date | Resolution     |
/// |          |---------------------------------------|
/// |          |                                       |
/// |          |     Thumbnail Grid View               |
/// |          |                                       |
/// +--------------------------------------------------+
/// |  Path: C:\...  |  10 file(s)  |  Thumbnails: 10  |
/// +--------------------------------------------------+
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// @brief Create the window
    /// @param hInstance Application instance
    /// @return true if created successfully
    bool create(HINSTANCE hInstance);

    /// @brief Show the window
    /// @param maximized Show maximized
    void show(bool maximized = false);

    /// @brief Get window handle
    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

    /// @brief Save window state to settings
    void saveState(config::Settings& settings) const;

    /// @brief Get directory tree component
    [[nodiscard]] DirectoryTree* directoryTree() noexcept { return tree_.get(); }

    /// @brief Get file list view component
    [[nodiscard]] FileListView* fileListView() noexcept { return file_list_.get(); }

    /// @brief Get thumbnail grid component
    [[nodiscard]] ThumbnailGrid* thumbnailGrid() noexcept { return grid_.get(); }

    /// @brief Update status bar from current application state
    void updateStatusBar();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void onCreate();
    void onSize(int width, int height);
    void onPaint();
    void onDestroy();
    void onCommand(WORD id);

    void createMenu();
    void createStatusBar();
    void createChildControls();
    void updateLayout();
    void applyFont();
    void updateSortMenu();

    // Vertical splitter (between tree and right pane)
    void onVsplitterDragStart(int x);
    void onVsplitterDrag(int x);
    void onVsplitterDragEnd();

    // Horizontal splitter (between file list and thumbnail grid)
    void onHsplitterDragStart(int y);
    void onHsplitterDrag(int y);
    void onHsplitterDragEnd();

    void drawSplitters(HDC hdc);

    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    HMENU menu_ = nullptr;
    HWND status_bar_ = nullptr;
    HFONT ui_font_ = nullptr;

    // Child controls
    std::unique_ptr<DirectoryTree> tree_;
    std::unique_ptr<FileListView> file_list_;
    std::unique_ptr<ThumbnailGrid> grid_;
    std::unique_ptr<FileOperationManager> file_op_manager_;

    // Layout - vertical splitter (left-right)
    int vsplitter_pos_ = 250;
    bool dragging_vsplitter_ = false;
    int vdrag_start_x_ = 0;
    int vdrag_start_pos_ = 0;

    // Layout - horizontal splitter (top-bottom in right pane)
    int hsplitter_pos_ = 200;  // Distance from top of right pane
    bool dragging_hsplitter_ = false;
    int hdrag_start_y_ = 0;
    int hdrag_start_pos_ = 0;

    static constexpr int kSplitterWidth = 5;
    static constexpr int kMinPaneWidth = 100;
    static constexpr int kMinPaneHeight = 100;
    static constexpr int kStatusBarHeight = 22;

    // Menu IDs
    static constexpr WORD kIdFileSettings = 1001;
    static constexpr WORD kIdFileRefresh = 1002;
    static constexpr WORD kIdFileExit = 1003;
    static constexpr WORD kIdViewThumbnails = 1101;
    static constexpr WORD kIdViewDetails = 1102;
    // Sort Method submenu
    static constexpr WORD kIdSortNatural = 1201;
    static constexpr WORD kIdSortLexicographic = 1202;
    static constexpr WORD kIdSortDateModified = 1203;
    static constexpr WORD kIdSortDateCreated = 1204;
    static constexpr WORD kIdSortSize = 1205;
    // Sort Order submenu
    static constexpr WORD kIdSortAscending = 1211;
    static constexpr WORD kIdSortDescending = 1212;

#ifdef NIVE_DEBUG_D2D_TEST
    // Help menu (Debug builds only)
    static constexpr WORD kIdDebugD2DTest = 9001;
#endif

    // Control IDs
    static constexpr int kIdDirectoryTree = 101;
    static constexpr int kIdFileList = 102;
    static constexpr int kIdThumbnailGrid = 103;
    static constexpr int kIdStatusBar = 104;
};

}  // namespace nive::ui
