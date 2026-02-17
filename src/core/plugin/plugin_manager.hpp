/// @file plugin_manager.hpp
/// @brief Plugin lifecycle management

#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "image/decoded_image.hpp"
#include "plugin_loader.hpp"

namespace nive::plugin {

/// @brief Configuration for plugin manager
struct PluginManagerConfig {
    std::filesystem::path pluginsDirectory;  // Directory to scan for plugins
    bool auto_load = true;                   // Automatically load plugins on init
};

/// @brief Plugin information for external queries
struct PluginInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::filesystem::path path;
    std::vector<std::string> extensions;
    bool loaded = false;
    bool has_settings = false;
};

/// @brief Manages plugin discovery, loading, and lifecycle
///
/// Thread-safe for all public methods.
class PluginManager {
public:
    /// @brief Construct plugin manager with configuration
    explicit PluginManager(const PluginManagerConfig& config = {});

    ~PluginManager();

    // Non-copyable, non-movable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    /// @brief Initialize the plugin manager
    ///
    /// Scans the plugins directory and optionally loads all plugins.
    /// @return true on success
    bool initialize();

    /// @brief Shutdown and unload all plugins
    void shutdown();

    /// @brief Scan plugins directory for available plugins
    /// @return Number of plugins found
    size_t scanPlugins();

    /// @brief Load a specific plugin by path
    /// @param path Path to the plugin DLL
    /// @return true on success
    bool loadPlugin(const std::filesystem::path& path);

    /// @brief Unload a specific plugin
    /// @param name Plugin name
    /// @return true if plugin was found and unloaded
    bool unloadPlugin(const std::string& name);

    /// @brief Reload a specific plugin
    /// @param name Plugin name
    /// @return true on success
    bool reloadPlugin(const std::string& name);

    /// @brief Get list of all discovered plugins
    [[nodiscard]] std::vector<PluginInfo> listPlugins() const;

    /// @brief Get list of loaded plugins
    [[nodiscard]] std::vector<PluginInfo> loadedPlugins() const;

    /// @brief Find plugins that can decode the given extension
    /// @param extension File extension (e.g., ".webp")
    /// @return List of plugin names that support the extension
    [[nodiscard]] std::vector<std::string> findDecoders(const std::string& extension) const;

    /// @brief Decode image using available plugins
    /// @param data Image data
    /// @param size Data size in bytes
    /// @param extension File extension hint
    /// @return Decoded image on success, nullopt on failure
    [[nodiscard]] std::optional<image::DecodedImage> decode(const uint8_t* data, size_t size,
                                                            const std::string& extension) const;

    /// @brief Check if any plugins support the given extension
    [[nodiscard]] bool supportsExtension(const std::string& extension) const;

    /// @brief Get number of loaded plugins
    [[nodiscard]] size_t loadedCount() const;

    /// @brief Get plugins directory
    [[nodiscard]] const std::filesystem::path& pluginsDirectory() const noexcept {
        return config_.pluginsDirectory;
    }

    /// @brief Set plugins directory and rescan
    void set_plugins_directory(const std::filesystem::path& path);

private:
    /// @brief Create PluginInfo from loaded plugin
    [[nodiscard]] PluginInfo make_plugin_info(const PluginLoader& loader) const;

    /// @brief Convert NiveDecodedImage to DecodedImage
    [[nodiscard]] std::optional<image::DecodedImage>
    convertImage(const NiveDecodedImage& src) const;

    PluginManagerConfig config_;
    mutable std::recursive_mutex mutex_;

    // Loaded plugins indexed by name
    std::unordered_map<std::string, std::unique_ptr<PluginLoader>> plugins_;

    // Discovered plugin paths (not yet loaded)
    std::vector<std::filesystem::path> discovered_;

    // Extension to plugin name mapping for quick lookup
    std::unordered_map<std::string, std::vector<std::string>> extensionMap_;

    bool initialized_ = false;
};

}  // namespace nive::plugin
