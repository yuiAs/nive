/// @file settings.hpp
/// @brief Application settings structure definitions

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nive::config {

/// @brief Cache storage location
enum class CacheLocation {
    AppData,   // %LOCALAPPDATA%/nive/cache
    Portable,  // <exe_dir>/cache
    Custom,    // User-specified path
};

/// @brief Get string representation of CacheLocation
[[nodiscard]] constexpr std::string_view to_string(CacheLocation loc) noexcept {
    switch (loc) {
    case CacheLocation::AppData:
        return "appdata";
    case CacheLocation::Portable:
        return "portable";
    case CacheLocation::Custom:
        return "custom";
    }
    return "appdata";
}

/// @brief Parse CacheLocation from string
[[nodiscard]] constexpr CacheLocation cacheLocationFromString(std::string_view str) noexcept {
    if (str == "portable")
        return CacheLocation::Portable;
    if (str == "custom")
        return CacheLocation::Custom;
    return CacheLocation::AppData;
}

/// @brief File sorting method
enum class SortMethod {
    Lexicographic,  // Standard string comparison
    Natural,        // Numeric-aware (file1 < file2 < file10)
    DateModified,   // By modification time
    DateCreated,    // By creation time
    Size,           // By file size
};

/// @brief Get string representation of SortMethod
[[nodiscard]] constexpr std::string_view to_string(SortMethod method) noexcept {
    switch (method) {
    case SortMethod::Lexicographic:
        return "lexicographic";
    case SortMethod::Natural:
        return "natural";
    case SortMethod::DateModified:
        return "date_modified";
    case SortMethod::DateCreated:
        return "date_created";
    case SortMethod::Size:
        return "size";
    }
    return "natural";
}

/// @brief Parse SortMethod from string
[[nodiscard]] constexpr SortMethod sortMethodFromString(std::string_view str) noexcept {
    if (str == "lexicographic")
        return SortMethod::Lexicographic;
    if (str == "date_modified")
        return SortMethod::DateModified;
    if (str == "date_created")
        return SortMethod::DateCreated;
    if (str == "size")
        return SortMethod::Size;
    return SortMethod::Natural;
}

/// @brief Image viewer display mode
enum class ViewerDisplayMode {
    Original,     // 1:1 pixel ratio
    FitToWindow,  // Scale to fit window (may upscale)
    ShrinkToFit,  // Scale down only (never upscale)
};

/// @brief Get string representation of ViewerDisplayMode
[[nodiscard]] constexpr std::string_view to_string(ViewerDisplayMode mode) noexcept {
    switch (mode) {
    case ViewerDisplayMode::Original:
        return "original";
    case ViewerDisplayMode::FitToWindow:
        return "fit";
    case ViewerDisplayMode::ShrinkToFit:
        return "shrink";
    }
    return "shrink";
}

/// @brief Parse ViewerDisplayMode from string
[[nodiscard]] constexpr ViewerDisplayMode displayModeFromString(std::string_view str) noexcept {
    if (str == "original")
        return ViewerDisplayMode::Original;
    if (str == "fit")
        return ViewerDisplayMode::FitToWindow;
    return ViewerDisplayMode::ShrinkToFit;
}

/// @brief Startup directory option
enum class StartupDirectory {
    Home,        // User's home folder (Pictures)
    LastOpened,  // Last opened directory (restore previous session)
    Custom,      // User-specified path
};

/// @brief Get string representation of StartupDirectory
[[nodiscard]] constexpr std::string_view to_string(StartupDirectory dir) noexcept {
    switch (dir) {
    case StartupDirectory::Home:
        return "home";
    case StartupDirectory::LastOpened:
        return "last_opened";
    case StartupDirectory::Custom:
        return "custom";
    }
    return "last_opened";
}

/// @brief Parse StartupDirectory from string
[[nodiscard]] constexpr StartupDirectory startupDirectoryFromString(std::string_view str) noexcept {
    if (str == "home")
        return StartupDirectory::Home;
    if (str == "custom")
        return StartupDirectory::Custom;
    return StartupDirectory::LastOpened;
}

/// @brief File conflict resolution strategy
enum class ConflictResolution {
    Ask,         // Ask user each time
    Overwrite,   // Always overwrite
    Skip,        // Always skip
    Rename,      // Add suffix (file_1, file_2, etc.)
    NewerDate,   // Keep newer file
    LargerSize,  // Keep larger file
};

/// @brief Get string representation of ConflictResolution
[[nodiscard]] constexpr std::string_view to_string(ConflictResolution res) noexcept {
    switch (res) {
    case ConflictResolution::Ask:
        return "ask";
    case ConflictResolution::Overwrite:
        return "overwrite";
    case ConflictResolution::Skip:
        return "skip";
    case ConflictResolution::Rename:
        return "rename";
    case ConflictResolution::NewerDate:
        return "newer";
    case ConflictResolution::LargerSize:
        return "larger";
    }
    return "ask";
}

/// @brief Parse ConflictResolution from string
[[nodiscard]] constexpr ConflictResolution
conflictResolutionFromString(std::string_view str) noexcept {
    if (str == "overwrite")
        return ConflictResolution::Overwrite;
    if (str == "skip")
        return ConflictResolution::Skip;
    if (str == "rename")
        return ConflictResolution::Rename;
    if (str == "newer")
        return ConflictResolution::NewerDate;
    if (str == "larger")
        return ConflictResolution::LargerSize;
    return ConflictResolution::Ask;
}

/// @brief File list column widths
struct FileListColumnWidths {
    float name = 200.0f;
    float size = 80.0f;
    float date = 120.0f;
    float dimensions = 100.0f;
    float path = 150.0f;
};

/// @brief Window state for restoration
struct WindowState {
    int x = 100;
    int y = 100;
    int width = 1280;
    int height = 720;
    bool maximized = false;
    int splitter_pos = 250;   // Vertical splitter (left-right)
    int hsplitter_pos = 200;  // Horizontal splitter (top-bottom in right pane)
};

/// @brief Thumbnail settings
///
/// Note: stored_size is the maximum size of thumbnails saved to cache (e.g., 384x384).
/// display_size is the current size shown in the grid view and can be adjusted at runtime.
struct ThumbnailSettings {
    int stored_size = 384;   // Max thumbnail size stored in cache (64-2048)
    int display_size = 128;  // Current display size in grid view (adjustable at runtime)
    int buffer_count = 50;   // Number of thumbnails to keep outside visible range
    int worker_count = 4;    // Number of thumbnail generation threads (1-16)
};

/// @brief Cache settings
struct CacheSettings {
    CacheLocation location = CacheLocation::AppData;
    std::filesystem::path custom_path;  // Used when location == Custom
    uint64_t max_size_mb = 500;
    uint64_t max_entries = 10000;
    int compression_level = 3;       // 0=off, 1-19=zstd levels
    bool retention_enabled = false;  // Enable automatic cache cleanup
    int retention_days = 30;         // Days to keep cache entries (when enabled)
};

/// @brief Sorting settings
struct SortSettings {
    SortMethod method = SortMethod::Natural;
    bool ascending = true;

    /// @brief Convert to fs::SortOrder compatible integer
    /// Maps SortMethod + ascending flag to fs::SortOrder enum value
    [[nodiscard]] int toSortOrder() const noexcept {
        // fs::SortOrder values:
        // Natural=0, NaturalDesc=1, Name=2, NameDesc=3, Size=4, SizeDesc=5,
        // Modified=6, ModifiedDesc=7, Type=8, TypeDesc=9
        int base = 0;
        switch (method) {
        case SortMethod::Natural:
            base = 0;  // Natural
            break;
        case SortMethod::Lexicographic:
            base = 2;  // Name
            break;
        case SortMethod::Size:
            base = 4;  // Size
            break;
        case SortMethod::DateModified:
            base = 6;  // Modified
            break;
        case SortMethod::DateCreated:
            base = 6;  // Use Modified as fallback (no DateCreated in SortOrder)
            break;
        }
        return ascending ? base : base + 1;
    }
};

/// @brief Application settings
struct Settings {
    // Thumbnail settings
    ThumbnailSettings thumbnails;

    // Cache settings
    CacheSettings cache;

    // Sorting settings
    SortSettings sort;

    // Network shares (additional root directories as UTF-8 strings)
    std::vector<std::string> network_shares;

    // UI layout
    FileListColumnWidths file_list_columns;

    // Viewer settings
    ViewerDisplayMode viewer_display_mode = ViewerDisplayMode::ShrinkToFit;
    WindowState main_window;
    WindowState viewer_window;

    // File operations
    ConflictResolution conflict_resolution = ConflictResolution::Ask;
    bool confirm_delete = true;
    bool use_recycle_bin = true;
    bool show_hidden_files = false;
    bool show_images_only = true;  // Filter to show only supported image formats

    // Startup directory settings
    StartupDirectory startup_directory = StartupDirectory::LastOpened;
    std::filesystem::path custom_startup_path;  // Used when startup_directory == Custom

    // Last directory (for restoring previous session)
    std::filesystem::path last_directory;

    // Language setting ("auto" = system detection, or explicit tag like "en", "ja")
    std::string language = "auto";

    /// @brief Get default settings
    [[nodiscard]] static Settings defaults() { return Settings{}; }
};

}  // namespace nive::config
