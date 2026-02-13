/// @file app.hpp
/// @brief Main application class

#pragma once

#include <Windows.h>

#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "core/archive/archive_manager.hpp"
#include "core/cache/cache_manager.hpp"
#include "core/config/settings.hpp"
#include "core/thumbnail/thumbnail_generator.hpp"
#include "core/thumbnail/thumbnail_request.hpp"
#include "state/app_state.hpp"

namespace nive::ui {

// Forward declarations
class MainWindow;
class ImageViewerWindow;

// Custom window message for thumbnail completion
constexpr UINT WM_THUMBNAIL_READY = WM_USER + 100;

/// @brief Application configuration
struct AppConfig {
    std::wstring initial_path;
    bool start_maximized = false;
};

/// @brief Main application class
///
/// Manages application lifecycle, creates main window, and
/// coordinates between UI and core modules.
class App {
public:
    /// @brief Get singleton instance
    [[nodiscard]] static App& instance();

    /// @brief Initialize application
    /// @param hInstance Application instance handle
    /// @param config Application configuration
    /// @return true if initialization succeeded
    bool initialize(HINSTANCE hInstance, const AppConfig& config = {});

    /// @brief Run application message loop
    /// @return Exit code
    int run();

    /// @brief Shutdown application
    void shutdown();

    /// @brief Get application state
    [[nodiscard]] AppState& state() noexcept { return *state_; }

    /// @brief Get settings
    [[nodiscard]] config::Settings& settings() noexcept { return settings_; }

    /// @brief Get cache manager
    [[nodiscard]] cache::CacheManager* cache() noexcept { return cache_.get(); }

    /// @brief Get archive manager
    [[nodiscard]] archive::ArchiveManager* archive() noexcept { return archive_.get(); }

    /// @brief Get thumbnail generator
    [[nodiscard]] thumbnail::ThumbnailGenerator* thumbnails() noexcept { return thumbnails_.get(); }

    /// @brief Get main window handle
    [[nodiscard]] HWND mainHwnd() const noexcept;

    /// @brief Get application instance handle
    [[nodiscard]] HINSTANCE hinstance() const noexcept { return hinstance_; }

    /// @brief Navigate to path
    void navigateTo(const std::filesystem::path& path);

    /// @brief Open image in viewer
    void openImage(const archive::VirtualPath& path);

    /// @brief Refresh current directory
    void refresh();

    /// @brief Save settings
    void saveSettings();

    /// @brief Check if archive support is available
    [[nodiscard]] bool isArchiveSupportAvailable() const noexcept;

    /// @brief Request thumbnail for a file
    /// @param path File path
    /// @param priority Request priority
    void requestThumbnail(const std::filesystem::path& path,
                          thumbnail::Priority priority = thumbnail::Priority::Normal);

    /// @brief Request thumbnail for a virtual path (archive contents)
    /// @param path Virtual path
    /// @param priority Request priority
    void requestThumbnail(const archive::VirtualPath& path,
                          thumbnail::Priority priority = thumbnail::Priority::Normal);

    /// @brief Process pending thumbnail results (call from UI thread)
    void processThumbnailResults();

private:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool initializeCore();
    void loadDirectory(const std::filesystem::path& path);
    void loadArchive(const std::filesystem::path& archive_path);

    HINSTANCE hinstance_ = nullptr;

    config::Settings settings_;
    std::unique_ptr<AppState> state_;
    std::unique_ptr<MainWindow> main_window_;
    std::unique_ptr<ImageViewerWindow> viewer_window_;

    std::unique_ptr<cache::CacheManager> cache_;
    std::unique_ptr<archive::ArchiveManager> archive_;
    std::unique_ptr<thumbnail::ThumbnailGenerator> thumbnails_;

    // Thread-safe queue for thumbnail results from worker threads
    mutable std::mutex thumbnail_queue_mutex_;
    std::queue<thumbnail::ThumbnailResult> thumbnail_results_;
};

}  // namespace nive::ui
