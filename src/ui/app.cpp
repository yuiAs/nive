/// @file app.cpp
/// @brief Main application implementation

#include "app.hpp"

#include <ShlObj.h>

#include "components/file_list_view.hpp"
#include "components/thumbnail_grid.hpp"
#include "core/archive/archive_entry.hpp"
#include "core/config/settings_manager.hpp"
#include "core/fs/directory.hpp"
#include "core/i18n/i18n.hpp"
#include "core/util/logger.hpp"
#include "image_viewer_window.hpp"
#include "main_window.hpp"

namespace nive::ui {

App& App::instance() {
    static App app;
    return app;
}

App::App() = default;
App::~App() = default;

bool App::initialize(HINSTANCE hInstance, const AppConfig& config) {
    hinstance_ = hInstance;

    // Initialize OLE for drag and drop
    HRESULT hr = OleInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_WARN("OleInitialize failed with HRESULT {:08X}, drag-drop may not work", hr);
    }

    // Load settings
    settings_ = config::SettingsManager::loadOrDefault();

    // Initialize i18n (before any UI creation)
    {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        auto locale_dir = std::filesystem::path(exe_path).parent_path() / L"locales";
        auto result = i18n::init(locale_dir, settings_.language);
        if (!result) {
            LOG_WARN("i18n init failed: falling back to key strings");
        }
    }

    // Create application state
    state_ = std::make_unique<AppState>();

    // Initialize core modules
    if (!initializeCore()) {
        // Non-fatal - continue with limited functionality
    }

    // Start thumbnail generator
    if (thumbnails_) {
        thumbnails_->start();
    }

    // Create main window
    main_window_ = std::make_unique<MainWindow>();
    if (!main_window_->create(hInstance)) {
        MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_ICONERROR);
        return false;
    }

    // Show window
    main_window_->show(config.start_maximized || settings_.main_window.maximized);

    // Navigate to initial path
    std::filesystem::path initial_path;
    if (!config.initial_path.empty()) {
        // Command-line argument takes precedence
        initial_path = config.initial_path;
    } else {
        // Use startup directory setting
        switch (settings_.startup_directory) {
        case config::StartupDirectory::Home: {
            auto pictures = fs::getSpecialFolder(CSIDL_MYPICTURES);
            if (pictures) {
                initial_path = *pictures;
            }
            break;
        }
        case config::StartupDirectory::LastOpened:
            if (!settings_.last_directory.empty() &&
                std::filesystem::exists(settings_.last_directory)) {
                initial_path = settings_.last_directory;
            }
            break;
        case config::StartupDirectory::Custom:
            if (!settings_.custom_startup_path.empty() &&
                std::filesystem::exists(settings_.custom_startup_path)) {
                initial_path = settings_.custom_startup_path;
            }
            break;
        }

        // Fallback to Pictures folder if no valid path determined
        if (initial_path.empty()) {
            auto pictures = fs::getSpecialFolder(CSIDL_MYPICTURES);
            if (pictures) {
                initial_path = *pictures;
            }
        }
    }

    if (!initial_path.empty()) {
        navigateTo(initial_path);
    }

    return true;
}

int App::run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // Handle accelerators if any
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

void App::shutdown() {
    // Save window state
    if (main_window_) {
        main_window_->saveState(settings_);
    }
    if (viewer_window_) {
        viewer_window_->saveState(settings_);
    }

    // Save current directory
    if (state_) {
        settings_.last_directory = state_->currentPath();
    }

    // Save settings
    saveSettings();

    // Cancel pending thumbnail requests and stop generator
    if (thumbnails_) {
        thumbnails_->cancelAll();
        thumbnails_->stop();
    }

    // Shutdown plugin manager
    if (plugins_) {
        plugins_->shutdown();
    }

    // Uninitialize OLE
    OleUninitialize();

    // Note: Don't reset main_window_ here - this may be called from within
    // the window procedure. Let the unique_ptrs clean up when App is destroyed.
}

HWND App::mainHwnd() const noexcept {
    return main_window_ ? main_window_->hwnd() : nullptr;
}

void App::navigateTo(const std::filesystem::path& path) {
    // Handle archives first (they exist as files)
    if (archive_ && archive_->isAvailable() && archive_->isArchive(path)) {
        state_->setCurrentPath(path);
        loadArchive(path);
        return;
    }

    if (!std::filesystem::exists(path)) {
        return;
    }

    if (std::filesystem::is_directory(path)) {
        state_->setCurrentPath(path);
        loadDirectory(path);
    } else if (fs::isImageExtension(path.extension().wstring())) {
        // Open image file - navigate to parent and open viewer
        auto parent = path.parent_path();
        if (state_->currentPath() != parent) {
            state_->setCurrentPath(parent);
            loadDirectory(parent);
        }
        openImage(archive::VirtualPath(path));
    }
}

void App::openImage(const archive::VirtualPath& path) {
    state_->setViewerImage(path);

    // Create viewer window if not exists
    if (!viewer_window_) {
        viewer_window_ = std::make_unique<ImageViewerWindow>();
        if (!viewer_window_->create(hinstance_)) {
            viewer_window_.reset();
            return;
        }
        viewer_window_->setDisplayMode(settings_.viewer_display_mode);
    }

    viewer_window_->setImage(path);
    viewer_window_->show();
}

void App::refresh() {
    auto path = state_->currentPath();
    if (!path.empty()) {
        loadDirectory(path);
    }
}

void App::saveSettings() {
    (void)config::SettingsManager::save(settings_);
}

bool App::isArchiveSupportAvailable() const noexcept {
    return archive_ && archive_->isAvailable();
}

bool App::initializeCore() {
    // Initialize cache
    cache::CacheConfig cache_config;
    cache_config.database_path = config::getCachePath(settings_);
    cache_config.memory_cache_size = 100;  // Items in memory
    cache_config.max_entries = settings_.cache.max_entries;
    cache_config.max_size_bytes = settings_.cache.max_size_mb * 1024 * 1024;
    cache_config.compression_level = settings_.cache.compression_level;

    auto cache_result = cache::CacheManager::create(cache_config);
    if (cache_result) {
        cache_ = std::move(*cache_result);
    }

    // Initialize archive manager
    archive::ArchiveManagerConfig archive_config;
    archive_ = std::make_unique<archive::ArchiveManager>(archive_config);

    // Initialize thumbnail generator
    thumbnail::GeneratorConfig thumb_config;
    thumb_config.default_thumbnail_size = settings_.thumbnails.stored_size;
    thumb_config.worker_count = static_cast<uint32_t>(settings_.thumbnails.worker_count);

    thumbnails_ = std::make_unique<thumbnail::ThumbnailGenerator>(thumb_config);

    // Connect cache manager to thumbnail generator
    if (cache_) {
        thumbnails_->setCacheManager(cache_.get());
    }

    // Initialize plugin manager
    {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        auto plugins_dir = std::filesystem::path(exe_path).parent_path() / L"plugins";

        plugin::PluginManagerConfig plugin_config;
        plugin_config.pluginsDirectory = plugins_dir;
        plugin_config.auto_load = true;

        plugins_ = std::make_unique<plugin::PluginManager>(plugin_config);
        if (plugins_->initialize()) {
            // Unload disabled plugins per settings
            for (const auto& name : settings_.plugins.disabled_plugins) {
                plugins_->unloadPlugin(name);
            }
            LOG_INFO("Plugin manager initialized: {} plugins loaded", plugins_->loadedCount());
        } else {
            LOG_WARN("Failed to initialize plugin manager");
        }
    }

    // Connect plugin manager to thumbnail generator
    if (plugins_ && thumbnails_) {
        thumbnails_->setPluginManager(plugins_.get());
    }

    return true;
}

void App::loadDirectory(const std::filesystem::path& path) {
    fs::DirectoryFilter filter;
    filter.include_hidden = settings_.show_hidden_files;
    filter.images_only = settings_.show_images_only;

    auto result =
        fs::scanDirectory(path, filter, static_cast<fs::SortOrder>(settings_.sort.toSortOrder()));
    if (result) {
        // Cancel any pending thumbnail requests for old directory
        // MUST be done BEFORE setting new files, which triggers new thumbnail requests
        if (thumbnails_) {
            thumbnails_->cancelAll();
        }

        state_->setFiles(std::move(result->entries));
    }
}

void App::loadArchive(const std::filesystem::path& archive_path) {
    if (!archive_ || !archive_->isAvailable()) {
        return;
    }

    // Cancel any pending thumbnail requests
    if (thumbnails_) {
        thumbnails_->cancelAll();
    }

    auto entries_result = archive_->getImageEntries(archive_path);
    if (!entries_result) {
        LOG_WARN("Failed to load archive {}: {}", archive_path.string(),
                 to_string(entries_result.error()));
        state_->setFiles({});
        return;
    }

    std::vector<fs::FileMetadata> files;
    for (const auto& entry : *entries_result) {
        if (entry.is_directory) {
            continue;
        }

        fs::FileMetadata meta;
        meta.virtual_path = archive::VirtualPath(archive_path, entry.path);
        meta.path = archive_path / entry.path;
        meta.name = entry.name;
        meta.extension = entry.extension();
        meta.type = fs::FileType::Image;
        meta.size_bytes = entry.uncompressed_size;
        meta.modified_time = entry.modified_time;

        files.push_back(std::move(meta));
    }

    LOG_INFO("Loaded {} images from archive {}", files.size(), archive_path.string());
    state_->setFiles(std::move(files));
}

void App::requestThumbnail(const std::filesystem::path& path, thumbnail::Priority priority) {
    if (!thumbnails_ || !thumbnails_->isRunning()) {
        return;
    }

    HWND hwnd = mainHwnd();
    if (!hwnd) {
        return;
    }

    // Request thumbnail with callback (ignore request ID)
    (void)thumbnails_->request(
        path,
        [this, hwnd](thumbnail::ThumbnailResult result) {
            // This callback runs on worker thread - queue result and notify UI thread
            {
                std::lock_guard lock(thumbnail_queue_mutex_);
                thumbnail_results_.push(std::move(result));
            }
            // Post message to UI thread
            PostMessageW(hwnd, WM_THUMBNAIL_READY, 0, 0);
        },
        priority, static_cast<uint32_t>(settings_.thumbnails.stored_size));
}

void App::requestThumbnail(const archive::VirtualPath& vpath, thumbnail::Priority priority) {
    if (!thumbnails_ || !thumbnails_->isRunning()) {
        return;
    }

    HWND hwnd = mainHwnd();
    if (!hwnd) {
        return;
    }

    if (vpath.is_in_archive()) {
        // Extract from archive and request thumbnail from memory
        if (!archive_) {
            return;
        }

        auto data = archive_->extractToMemory(vpath);
        if (!data) {
            std::filesystem::path vpath_as_path(vpath.to_string());
            LOG_WARN("Failed to extract {} from archive: {}", vpath_as_path.string(),
                     to_string(data.error()));
            return;
        }

        // Use virtual path string as the path identifier
        std::filesystem::path virtual_path_id(vpath.to_string());

        (void)thumbnails_->requestFromMemory(
            virtual_path_id, std::move(*data),
            [this, hwnd, vpath](thumbnail::ThumbnailResult result) {
                // Override path with virtual path string for correct keying
                result.path = std::filesystem::path(vpath.to_string());
                {
                    std::lock_guard lock(thumbnail_queue_mutex_);
                    thumbnail_results_.push(std::move(result));
                }
                PostMessageW(hwnd, WM_THUMBNAIL_READY, 0, 0);
            },
            priority, static_cast<uint32_t>(settings_.thumbnails.stored_size));
    } else {
        // Regular filesystem path
        requestThumbnail(vpath.archive_path(), priority);
    }
}

void App::processThumbnailResults() {
    std::queue<thumbnail::ThumbnailResult> results;

    // Swap queues under lock to minimize lock time
    {
        std::lock_guard lock(thumbnail_queue_mutex_);
        results.swap(thumbnail_results_);
    }

    if (results.empty()) {
        return;
    }

    LOG_DEBUG("processThumbnailResults: processing {} results", results.size());

    // Process results on UI thread
    size_t success_count = 0;
    while (!results.empty()) {
        auto result = std::move(results.front());
        results.pop();

        if (result.success() && result.thumbnail && main_window_) {
            if (auto* grid = main_window_->thumbnailGrid()) {
                grid->setThumbnail(result.path, std::move(*result.thumbnail));
                success_count++;
            }
            if (auto* list = main_window_->fileListView()) {
                list->setResolution(result.path, result.original_width, result.original_height);
            }
        } else if (result.error) {
            LOG_WARN("Thumbnail generation failed for {}: {}", result.path.string(), *result.error);
        }
    }

    LOG_DEBUG("processThumbnailResults: {} thumbnails set successfully", success_count);

    // Update status bar with current thumbnail count
    if (main_window_) {
        main_window_->updateStatusBar();
    }
}

}  // namespace nive::ui
