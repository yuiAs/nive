/// @file settings_manager.cpp
/// @brief Settings persistence implementation using toml++

#include "settings_manager.hpp"
#include <Windows.h>

#include <ShlObj.h>

#include <fstream>
#include <sstream>

#ifdef NIVE_HAS_TOMLPLUSPLUS
    #include <toml++/toml.hpp>
#endif

namespace nive::config {

namespace {

// Helper to convert wstring to UTF-8 string
std::string to_utf8(const std::wstring& wstr) {
    if (wstr.empty())
        return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(),
                        size_needed, nullptr, nullptr);
    return result;
}

// Helper to convert UTF-8 string to wstring
std::wstring from_utf8(const std::string& str) {
    if (str.empty())
        return {};
    int size_needed =
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(),
                        size_needed);
    return result;
}

#ifdef NIVE_HAS_TOMLPLUSPLUS

// Helper to get optional value from toml table
template <typename T>
T get_or(const toml::table& tbl, std::string_view key, T default_value) {
    if (auto val = tbl[key].value<T>()) {
        return *val;
    }
    return default_value;
}

// Helper to get optional nested value
template <typename T>
T get_nested_or(const toml::table& tbl, std::string_view section, std::string_view key,
                T default_value) {
    if (auto* section_tbl = tbl[section].as_table()) {
        return get_or(*section_tbl, key, default_value);
    }
    return default_value;
}

Settings parse_settings(const toml::table& tbl) {
    Settings settings;

    // Thumbnail settings
    if (auto* thumbnails = tbl["thumbnails"].as_table()) {
        settings.thumbnails.stored_size = get_or(*thumbnails, "stored_size", 384);
        settings.thumbnails.display_size = get_or(*thumbnails, "display_size", 128);
        settings.thumbnails.buffer_count = get_or(*thumbnails, "buffer_count", 50);
        settings.thumbnails.worker_count = get_or(*thumbnails, "worker_count", 4);
    }

    // Cache settings
    if (auto* cache = tbl["cache"].as_table()) {
        auto location_str = get_or<std::string>(*cache, "location", "appdata");
        settings.cache.location = cacheLocationFromString(location_str);

        if (auto path_str = cache->get("custom_path")) {
            if (auto* str = path_str->as_string()) {
                settings.cache.custom_path = from_utf8(str->get());
            }
        }

        settings.cache.max_size_mb =
            static_cast<uint64_t>(get_or(*cache, "max_size_mb", int64_t{500}));
        settings.cache.max_entries =
            static_cast<uint64_t>(get_or(*cache, "max_entries", int64_t{10000}));
        settings.cache.compression_level = get_or(*cache, "compression_level", 3);
        settings.cache.retention_enabled = get_or(*cache, "retention_enabled", false);
        settings.cache.retention_days = get_or(*cache, "retention_days", 30);
    }

    // Sorting settings
    if (auto* sorting = tbl["sorting"].as_table()) {
        auto method_str = get_or<std::string>(*sorting, "method", "natural");
        settings.sort.method = sortMethodFromString(method_str);
        settings.sort.ascending = get_or(*sorting, "ascending", true);
    }

    // Network shares
    if (auto* network = tbl["network"].as_table()) {
        if (auto* shares = network->get("shares")) {
            if (auto* arr = shares->as_array()) {
                for (const auto& item : *arr) {
                    if (auto* str = item.as_string()) {
                        settings.network_shares.push_back(str->get());
                    }
                }
            }
        }
    }

    // Viewer settings
    if (auto* viewer = tbl["viewer"].as_table()) {
        auto mode_str = get_or<std::string>(*viewer, "display_mode", "shrink");
        settings.viewer_display_mode = displayModeFromString(mode_str);
    }

    // File operations
    if (auto* file_ops = tbl["file_operations"].as_table()) {
        auto res_str = get_or<std::string>(*file_ops, "conflict_resolution", "ask");
        settings.conflict_resolution = conflictResolutionFromString(res_str);
        settings.confirm_delete = get_or(*file_ops, "confirm_delete", true);
        settings.use_recycle_bin = get_or(*file_ops, "use_recycle_bin", true);
        settings.show_hidden_files = get_or(*file_ops, "show_hidden_files", false);
        settings.show_images_only = get_or(*file_ops, "show_images_only", true);
    }

    // Main window state
    if (auto* main_win = tbl["main_window"].as_table()) {
        settings.main_window.x = get_or(*main_win, "x", 100);
        settings.main_window.y = get_or(*main_win, "y", 100);
        settings.main_window.width = get_or(*main_win, "width", 1280);
        settings.main_window.height = get_or(*main_win, "height", 720);
        settings.main_window.maximized = get_or(*main_win, "maximized", false);
        settings.main_window.splitter_pos = get_or(*main_win, "splitter_pos", 250);
        settings.main_window.hsplitter_pos = get_or(*main_win, "hsplitter_pos", 200);
    }

    // File list column widths
    if (auto* columns = tbl["file_list_columns"].as_table()) {
        settings.file_list_columns.name = static_cast<float>(get_or(*columns, "name", 200.0));
        settings.file_list_columns.size = static_cast<float>(get_or(*columns, "size", 80.0));
        settings.file_list_columns.date = static_cast<float>(get_or(*columns, "date", 120.0));
        settings.file_list_columns.dimensions =
            static_cast<float>(get_or(*columns, "dimensions", 100.0));
    }

    // Viewer window state
    if (auto* viewer_win = tbl["viewer_window"].as_table()) {
        settings.viewer_window.x = get_or(*viewer_win, "x", 100);
        settings.viewer_window.y = get_or(*viewer_win, "y", 100);
        settings.viewer_window.width = get_or(*viewer_win, "width", 1280);
        settings.viewer_window.height = get_or(*viewer_win, "height", 720);
        settings.viewer_window.maximized = get_or(*viewer_win, "maximized", false);
        settings.viewer_window.splitter_pos = get_or(*viewer_win, "splitter_pos", 250);
        settings.viewer_window.hsplitter_pos = get_or(*viewer_win, "hsplitter_pos", 200);
    }

    // Startup settings
    if (auto* startup = tbl["startup"].as_table()) {
        auto dir_str = get_or<std::string>(*startup, "directory", "last_opened");
        settings.startup_directory = startupDirectoryFromString(dir_str);

        if (auto path_str = startup->get("custom_path")) {
            if (auto* str = path_str->as_string()) {
                settings.custom_startup_path = from_utf8(str->get());
            }
        }
    }

    // Application state
    if (auto* state = tbl["state"].as_table()) {
        if (auto path_str = state->get("last_directory")) {
            if (auto* str = path_str->as_string()) {
                settings.last_directory = from_utf8(str->get());
            }
        }
    }

    return settings;
}

toml::table serialize_settings(const Settings& settings) {
    toml::table tbl;

    // Thumbnail settings
    tbl.insert("thumbnails", toml::table{
                                 { "stored_size",  settings.thumbnails.stored_size},
                                 {"display_size", settings.thumbnails.display_size},
                                 {"buffer_count", settings.thumbnails.buffer_count},
                                 {"worker_count", settings.thumbnails.worker_count},
    });

    // Cache settings
    toml::table cache_tbl{
        {         "location",  std::string(to_string(settings.cache.location))},
        {      "max_size_mb", static_cast<int64_t>(settings.cache.max_size_mb)},
        {      "max_entries", static_cast<int64_t>(settings.cache.max_entries)},
        {"compression_level",                 settings.cache.compression_level},
        {"retention_enabled",                 settings.cache.retention_enabled},
        {   "retention_days",                    settings.cache.retention_days},
    };
    if (!settings.cache.custom_path.empty()) {
        cache_tbl.insert("custom_path", to_utf8(settings.cache.custom_path.wstring()));
    }
    tbl.insert("cache", std::move(cache_tbl));

    // Sorting settings
    tbl.insert("sorting", toml::table{
                              {   "method", std::string(to_string(settings.sort.method))},
                              {"ascending",                      settings.sort.ascending},
    });

    // Network shares
    if (!settings.network_shares.empty()) {
        toml::array shares_arr;
        for (const auto& share : settings.network_shares) {
            shares_arr.push_back(share);
        }
        tbl.insert("network", toml::table{
                                  {"shares", std::move(shares_arr)}
        });
    }

    // Viewer settings
    tbl.insert("viewer", toml::table{
                             {"display_mode", std::string(to_string(settings.viewer_display_mode))},
    });

    // File operations
    tbl.insert("file_operations",
               toml::table{
                   {"conflict_resolution", std::string(to_string(settings.conflict_resolution))},
                   {     "confirm_delete",                              settings.confirm_delete},
                   {    "use_recycle_bin",                             settings.use_recycle_bin},
                   {  "show_hidden_files",                           settings.show_hidden_files},
                   {   "show_images_only",                            settings.show_images_only},
    });

    // Main window state
    tbl.insert("main_window", toml::table{
                                  {            "x",             settings.main_window.x},
                                  {            "y",             settings.main_window.y},
                                  {        "width",         settings.main_window.width},
                                  {       "height",        settings.main_window.height},
                                  {    "maximized",     settings.main_window.maximized},
                                  { "splitter_pos",  settings.main_window.splitter_pos},
                                  {"hsplitter_pos", settings.main_window.hsplitter_pos},
    });

    // File list column widths
    tbl.insert("file_list_columns",
               toml::table{
                   {      "name",       static_cast<double>(settings.file_list_columns.name)},
                   {      "size",       static_cast<double>(settings.file_list_columns.size)},
                   {      "date",       static_cast<double>(settings.file_list_columns.date)},
                   {"dimensions", static_cast<double>(settings.file_list_columns.dimensions)},
    });

    // Viewer window state
    tbl.insert("viewer_window", toml::table{
                                    {            "x",             settings.viewer_window.x},
                                    {            "y",             settings.viewer_window.y},
                                    {        "width",         settings.viewer_window.width},
                                    {       "height",        settings.viewer_window.height},
                                    {    "maximized",     settings.viewer_window.maximized},
                                    { "splitter_pos",  settings.viewer_window.splitter_pos},
                                    {"hsplitter_pos", settings.viewer_window.hsplitter_pos},
    });

    // Startup settings
    toml::table startup_tbl{
        {"directory", std::string(to_string(settings.startup_directory))},
    };
    if (!settings.custom_startup_path.empty()) {
        startup_tbl.insert("custom_path", to_utf8(settings.custom_startup_path.wstring()));
    }
    tbl.insert("startup", std::move(startup_tbl));

    // Application state
    if (!settings.last_directory.empty()) {
        tbl.insert("state", toml::table{
                                {"last_directory", to_utf8(settings.last_directory.wstring())},
        });
    }

    return tbl;
}

#endif  // NIVE_HAS_TOMLPLUSPLUS

}  // namespace

std::filesystem::path SettingsManager::defaultPath() {
    // Try %LOCALAPPDATA%/nive/settings.toml
    wchar_t* local_app_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data))) {
        auto path = std::filesystem::path(local_app_data) / L"nive" / L"settings.toml";
        CoTaskMemFree(local_app_data);
        return path;
    }

    // Fallback to executable directory
    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
        return std::filesystem::path(exe_path).parent_path() / L"settings.toml";
    }

    return L"settings.toml";
}

std::expected<Settings, ConfigError> SettingsManager::load() {
    return loadFrom(defaultPath());
}

std::expected<Settings, ConfigError> SettingsManager::loadFrom(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(ConfigError::FileNotFound);
    }

#ifdef NIVE_HAS_TOMLPLUSPLUS
    try {
        auto tbl = toml::parse_file(path.string());
        return parse_settings(tbl);
    } catch (const toml::parse_error&) {
        return std::unexpected(ConfigError::ParseError);
    }
#else
    // Without toml++, return defaults
    return Settings::defaults();
#endif
}

Settings SettingsManager::loadOrDefault() {
    auto result = load();
    if (result) {
        return *result;
    }
    return Settings::defaults();
}

std::expected<void, ConfigError> SettingsManager::save(const Settings& settings) {
    return saveTo(settings, defaultPath());
}

std::expected<void, ConfigError> SettingsManager::saveTo(const Settings& settings,
                                                         const std::filesystem::path& path) {
    try {
        // Create parent directories
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path);
        if (!file) {
            return std::unexpected(ConfigError::IoError);
        }

#ifdef NIVE_HAS_TOMLPLUSPLUS
        // Use toml++ for serialization
        auto tbl = serialize_settings(settings);
        file << "# nive configuration\n\n" << tbl;
#else
        // Fallback to manual TOML writing (legacy code path)
        file << "# nive configuration\n\n";

        // Thumbnail settings
        file << "[thumbnails]\n";
        file << "stored_size = " << settings.thumbnails.stored_size << "\n";
        file << "display_size = " << settings.thumbnails.display_size << "\n";
        file << "buffer_count = " << settings.thumbnails.buffer_count << "\n";
        file << "worker_count = " << settings.thumbnails.worker_count << "\n";
        file << "\n";

        // Cache settings
        file << "[cache]\n";
        file << "location = \"" << to_string(settings.cache.location) << "\"\n";
        if (!settings.cache.custom_path.empty()) {
            file << "custom_path = \"" << to_utf8(settings.cache.custom_path.wstring()) << "\"\n";
        }
        file << "max_size_mb = " << settings.cache.max_size_mb << "\n";
        file << "max_entries = " << settings.cache.max_entries << "\n";
        file << "compression_level = " << settings.cache.compression_level << "\n";
        file << "retention_enabled = " << (settings.cache.retention_enabled ? "true" : "false")
             << "\n";
        file << "retention_days = " << settings.cache.retention_days << "\n";
        file << "\n";

        // Sorting settings
        file << "[sorting]\n";
        file << "method = \"" << to_string(settings.sort.method) << "\"\n";
        file << "ascending = " << (settings.sort.ascending ? "true" : "false") << "\n";
        file << "\n";

        // Network shares
        if (!settings.network_shares.empty()) {
            file << "[network]\n";
            file << "shares = [\n";
            for (size_t i = 0; i < settings.network_shares.size(); ++i) {
                file << "    \"" << settings.network_shares[i] << "\"";
                if (i + 1 < settings.network_shares.size()) {
                    file << ",";
                }
                file << "\n";
            }
            file << "]\n\n";
        }

        // Viewer settings
        file << "[viewer]\n";
        file << "display_mode = \"" << to_string(settings.viewer_display_mode) << "\"\n";
        file << "\n";

        // File operations
        file << "[file_operations]\n";
        file << "conflict_resolution = \"" << to_string(settings.conflict_resolution) << "\"\n";
        file << "confirm_delete = " << (settings.confirm_delete ? "true" : "false") << "\n";
        file << "use_recycle_bin = " << (settings.use_recycle_bin ? "true" : "false") << "\n";
        file << "show_hidden_files = " << (settings.show_hidden_files ? "true" : "false") << "\n";
        file << "show_images_only = " << (settings.show_images_only ? "true" : "false") << "\n";
        file << "\n";

        // Main window state
        file << "[main_window]\n";
        file << "x = " << settings.main_window.x << "\n";
        file << "y = " << settings.main_window.y << "\n";
        file << "width = " << settings.main_window.width << "\n";
        file << "height = " << settings.main_window.height << "\n";
        file << "maximized = " << (settings.main_window.maximized ? "true" : "false") << "\n";
        file << "splitter_pos = " << settings.main_window.splitter_pos << "\n";
        file << "hsplitter_pos = " << settings.main_window.hsplitter_pos << "\n";
        file << "\n";

        // File list column widths
        file << "[file_list_columns]\n";
        file << "name = " << static_cast<int>(settings.file_list_columns.name) << "\n";
        file << "size = " << static_cast<int>(settings.file_list_columns.size) << "\n";
        file << "date = " << static_cast<int>(settings.file_list_columns.date) << "\n";
        file << "dimensions = " << static_cast<int>(settings.file_list_columns.dimensions) << "\n";
        file << "\n";

        // Viewer window state
        file << "[viewer_window]\n";
        file << "x = " << settings.viewer_window.x << "\n";
        file << "y = " << settings.viewer_window.y << "\n";
        file << "width = " << settings.viewer_window.width << "\n";
        file << "height = " << settings.viewer_window.height << "\n";
        file << "maximized = " << (settings.viewer_window.maximized ? "true" : "false") << "\n";
        file << "\n";

        // Startup settings
        file << "[startup]\n";
        file << "directory = \"" << to_string(settings.startup_directory) << "\"\n";
        if (!settings.custom_startup_path.empty()) {
            file << "custom_path = \"" << to_utf8(settings.custom_startup_path.wstring()) << "\"\n";
        }
        file << "\n";

        // Application state
        if (!settings.last_directory.empty()) {
            file << "[state]\n";
            file << "last_directory = \"" << to_utf8(settings.last_directory.wstring()) << "\"\n";
        }
#endif

        return {};
    } catch (const std::exception&) {
        return std::unexpected(ConfigError::IoError);
    }
}

ValidationResult SettingsManager::validate(const Settings& settings) {
    ValidationResult result;

    // Thumbnail stored size
    if (settings.thumbnails.stored_size < 64 || settings.thumbnails.stored_size > 2048) {
        result.errors.push_back("thumbnails.stored_size must be between 64 and 2048");
        result.valid = false;
    }

    // Thumbnail display size
    if (settings.thumbnails.display_size < 32 || settings.thumbnails.display_size > 512) {
        result.errors.push_back("thumbnails.display_size must be between 32 and 512");
        result.valid = false;
    }

    // Thumbnail buffer count
    if (settings.thumbnails.buffer_count < 0 || settings.thumbnails.buffer_count > 1000) {
        result.errors.push_back("thumbnails.buffer_count must be between 0 and 1000");
        result.valid = false;
    }

    // Worker count
    if (settings.thumbnails.worker_count < 1 || settings.thumbnails.worker_count > 16) {
        result.errors.push_back("thumbnails.worker_count must be between 1 and 16");
        result.valid = false;
    }

    // Custom cache path
    if (settings.cache.location == CacheLocation::Custom && settings.cache.custom_path.empty()) {
        result.errors.push_back("cache.custom_path required when cache.location is custom");
        result.valid = false;
    }

    // Custom startup path
    if (settings.startup_directory == StartupDirectory::Custom &&
        settings.custom_startup_path.empty()) {
        result.errors.push_back("startup.custom_path required when startup.directory is custom");
        result.valid = false;
    }

    // Compression level
    if (settings.cache.compression_level < 0 || settings.cache.compression_level > 19) {
        result.errors.push_back("cache.compression_level must be between 0 and 19");
        result.valid = false;
    }

    // Retention days
    if (settings.cache.retention_enabled && settings.cache.retention_days < 1) {
        result.errors.push_back(
            "cache.retention_days must be at least 1 when retention is enabled");
        result.valid = false;
    }

    // Warnings
    if (settings.thumbnails.stored_size > 1024) {
        result.warnings.push_back("Large thumbnail size may increase cache size significantly");
    }

    if (settings.thumbnails.worker_count > 8) {
        result.warnings.push_back("High worker count may affect system performance");
    }

    return result;
}

std::filesystem::path getCachePath(const Settings& settings) {
    switch (settings.cache.location) {
    case CacheLocation::AppData: {
        wchar_t* local_app_data = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data))) {
            auto path = std::filesystem::path(local_app_data) / L"nive" / L"cache";
            CoTaskMemFree(local_app_data);
            return path;
        }
        // Fallback to temp directory
        return std::filesystem::temp_directory_path() / L"nive" / L"cache";
    }

    case CacheLocation::Portable: {
        wchar_t exe_path[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
            return std::filesystem::path(exe_path).parent_path() / L"cache";
        }
        return L"cache";
    }

    case CacheLocation::Custom:
        return settings.cache.custom_path;
    }

    return std::filesystem::temp_directory_path() / L"nive" / L"cache";
}

}  // namespace nive::config
