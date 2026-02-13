/// @file plugin_manager.cpp
/// @brief Plugin lifecycle management implementation

#include "plugin_manager.hpp"

#include <algorithm>

#include "image/decoded_image.hpp"

namespace nive::plugin {

PluginManager::PluginManager(const PluginManagerConfig& config) : config_(config) {
}

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::initialize() {
    std::lock_guard lock(mutex_);

    if (initialized_) {
        return true;
    }

    // Create plugins directory if it doesn't exist
    if (!config_.pluginsDirectory.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(config_.pluginsDirectory, ec);
    }

    initialized_ = true;

    // Scan for plugins
    scanPlugins();

    // Auto-load if configured
    if (config_.auto_load) {
        for (const auto& path : discovered_) {
            loadPlugin(path);
        }
    }

    return true;
}

void PluginManager::shutdown() {
    std::lock_guard lock(mutex_);

    // Clear extension map
    extensionMap_.clear();

    // Unload all plugins (destructors will handle shutdown)
    plugins_.clear();

    discovered_.clear();
    initialized_ = false;
}

size_t PluginManager::scanPlugins() {
    // Note: mutex should already be held by caller or we need to acquire it
    // For external calls, we'll acquire it
    std::lock_guard lock(mutex_);

    discovered_.clear();

    if (config_.pluginsDirectory.empty() || !std::filesystem::exists(config_.pluginsDirectory)) {
        return 0;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(config_.pluginsDirectory, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == L".dll") {
            discovered_.push_back(entry.path());
        }
    }

    return discovered_.size();
}

bool PluginManager::loadPlugin(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);

    // Try to load the plugin
    auto result = PluginLoader::load(path);
    if (!result) {
        return false;
    }

    auto loader = std::make_unique<PluginLoader>(std::move(*result));
    const NivePluginInfo* info = loader->info();

    if (!info || !info->name) {
        return false;
    }

    std::string name = info->name;

    // Check if already loaded
    if (plugins_.contains(name)) {
        // Unload existing first
        unloadPlugin(name);
    }

    // Register extensions
    if (info->supported_extensions) {
        for (size_t i = 0; i < info->extension_count; ++i) {
            if (info->supported_extensions[i]) {
                std::string ext = info->supported_extensions[i];
                extensionMap_[ext].push_back(name);
            }
        }
    }

    // Store the loader
    plugins_[name] = std::move(loader);

    return true;
}

bool PluginManager::unloadPlugin(const std::string& name) {
    std::lock_guard lock(mutex_);

    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }

    // Remove from extension map
    for (auto& [ext, names] : extensionMap_) {
        std::erase(names, name);
    }

    // Remove empty extension entries
    std::erase_if(extensionMap_, [](const auto& pair) { return pair.second.empty(); });

    // Unload (destructor handles cleanup)
    plugins_.erase(it);

    return true;
}

bool PluginManager::reloadPlugin(const std::string& name) {
    std::lock_guard lock(mutex_);

    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        return false;
    }

    auto path = it->second->path();

    // Unload
    unloadPlugin(name);

    // Reload
    return loadPlugin(path);
}

std::vector<PluginInfo> PluginManager::listPlugins() const {
    std::lock_guard lock(mutex_);

    std::vector<PluginInfo> result;

    // Add loaded plugins
    for (const auto& [name, loader] : plugins_) {
        result.push_back(make_plugin_info(*loader));
    }

    // Add discovered but not loaded plugins
    for (const auto& path : discovered_) {
        bool loaded = false;
        for (const auto& [name, loader] : plugins_) {
            if (loader->path() == path) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            PluginInfo info;
            info.path = path;
            info.name = path.stem().string();
            info.loaded = false;
            result.push_back(info);
        }
    }

    return result;
}

std::vector<PluginInfo> PluginManager::loadedPlugins() const {
    std::lock_guard lock(mutex_);

    std::vector<PluginInfo> result;
    for (const auto& [name, loader] : plugins_) {
        result.push_back(make_plugin_info(*loader));
    }

    return result;
}

std::vector<std::string> PluginManager::findDecoders(const std::string& extension) const {
    std::lock_guard lock(mutex_);

    std::string ext = extension;
    // Ensure lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });

    auto it = extensionMap_.find(ext);
    if (it != extensionMap_.end()) {
        return it->second;
    }

    return {};
}

std::optional<image::DecodedImage> PluginManager::decode(const uint8_t* data, size_t size,
                                                         const std::string& extension) const {
    std::lock_guard lock(mutex_);

    // Find plugins that support this extension
    auto decoders = findDecoders(extension);

    // Try each decoder
    for (const auto& name : decoders) {
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            continue;
        }

        auto result = it->second->decode(data, size, extension);
        if (result) {
            auto image = convertImage(*result);
            it->second->freeImage(*result);
            if (image) {
                return image;
            }
        }
    }

    // Try all plugins if extension-based lookup failed
    for (const auto& [name, loader] : plugins_) {
        if (loader->canDecode(extension)) {
            auto result = loader->decode(data, size, extension);
            if (result) {
                auto image = convertImage(*result);
                loader->freeImage(*result);
                if (image) {
                    return image;
                }
            }
        }
    }

    return std::nullopt;
}

bool PluginManager::supportsExtension(const std::string& extension) const {
    std::lock_guard lock(mutex_);

    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });

    if (extensionMap_.contains(ext)) {
        return true;
    }

    // Check each plugin
    for (const auto& [name, loader] : plugins_) {
        if (loader->canDecode(ext)) {
            return true;
        }
    }

    return false;
}

size_t PluginManager::loadedCount() const {
    std::lock_guard lock(mutex_);
    return plugins_.size();
}

void PluginManager::set_plugins_directory(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    config_.pluginsDirectory = path;

    if (initialized_) {
        scanPlugins();
    }
}

PluginInfo PluginManager::make_plugin_info(const PluginLoader& loader) const {
    PluginInfo info;
    info.path = loader.path();
    info.loaded = true;

    const NivePluginInfo* plugin_info = loader.info();
    if (plugin_info) {
        info.name = plugin_info->name ? plugin_info->name : "";
        info.version = plugin_info->version ? plugin_info->version : "";
        info.author = plugin_info->author ? plugin_info->author : "";
        info.description = plugin_info->description ? plugin_info->description : "";

        if (plugin_info->supported_extensions) {
            for (size_t i = 0; i < plugin_info->extension_count; ++i) {
                if (plugin_info->supported_extensions[i]) {
                    info.extensions.push_back(plugin_info->supported_extensions[i]);
                }
            }
        }
    }

    return info;
}

std::optional<image::DecodedImage> PluginManager::convertImage(const NiveDecodedImage& src) const {
    if (!src.pixels || src.width == 0 || src.height == 0) {
        return std::nullopt;
    }

    // Determine bytes per pixel and target format
    size_t bytes_per_pixel;
    image::PixelFormat format;

    switch (src.format) {
    case NIVE_PIXEL_FORMAT_RGBA8:
        bytes_per_pixel = 4;
        format = image::PixelFormat::RGBA32;
        break;
    case NIVE_PIXEL_FORMAT_RGB8:
        bytes_per_pixel = 3;
        format = image::PixelFormat::RGB24;
        break;
    case NIVE_PIXEL_FORMAT_GRAY8:
        bytes_per_pixel = 1;
        format = image::PixelFormat::Gray8;
        break;
    default:
        return std::nullopt;
    }

    // Calculate stride and copy pixels
    uint32_t stride = src.width * static_cast<uint32_t>(bytes_per_pixel);
    size_t total_size = stride * src.height;

    std::vector<uint8_t> pixels(total_size);
    std::memcpy(pixels.data(), src.pixels, total_size);

    return image::DecodedImage(src.width, src.height, format, stride, std::move(pixels));
}

}  // namespace nive::plugin
