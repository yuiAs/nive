/// @file plugin_loader.hpp
/// @brief Dynamic library loading for plugins

#pragma once

#include <Windows.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

#include "nive/plugin_api.h"

namespace nive::plugin {

/// @brief Plugin loading error types
enum class LoadError {
    FileNotFound,
    LoadFailed,
    MissingExport,
    InitFailed,
    VersionMismatch,
};

/// @brief Get string representation of LoadError
[[nodiscard]] constexpr std::string_view to_string(LoadError error) noexcept {
    switch (error) {
    case LoadError::FileNotFound:
        return "Plugin file not found";
    case LoadError::LoadFailed:
        return "Failed to load plugin DLL";
    case LoadError::MissingExport:
        return "Plugin missing required export";
    case LoadError::InitFailed:
        return "Plugin initialization failed";
    case LoadError::VersionMismatch:
        return "Plugin API version mismatch";
    }
    return "Unknown error";
}

/// @brief RAII wrapper for loaded plugin DLL
///
/// Manages the lifecycle of a dynamically loaded plugin, including
/// loading, initialization, and cleanup.
class PluginLoader {
public:
    /// @brief Load a plugin from the specified path
    /// @param path Path to the plugin DLL
    /// @return PluginLoader on success, LoadError on failure
    [[nodiscard]] static std::expected<PluginLoader, LoadError>
    load(const std::filesystem::path& path);

    ~PluginLoader();

    // Non-copyable
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    // Movable
    PluginLoader(PluginLoader&& other) noexcept;
    PluginLoader& operator=(PluginLoader&& other) noexcept;

    /// @brief Get plugin information
    [[nodiscard]] const NivePluginInfo* info() const noexcept { return info_; }

    /// @brief Get plugin file path
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    /// @brief Check if plugin can decode the given extension
    /// @param extension File extension (e.g., ".webp")
    /// @return true if supported
    [[nodiscard]] bool canDecode(const std::string& extension) const;

    /// @brief Decode image from memory buffer
    /// @param data Image data
    /// @param size Data size in bytes
    /// @param extension File extension hint (optional)
    /// @return Decoded image on success, error code on failure
    [[nodiscard]] std::expected<NiveDecodedImage, NivePluginError>
    decode(const uint8_t* data, size_t size, const std::string& extension = "") const;

    /// @brief Free decoded image data
    /// @param image Image to free
    void freeImage(NiveDecodedImage& image) const;

    /// @brief Check if plugin is valid/loaded
    [[nodiscard]] bool valid() const noexcept { return module_ != nullptr; }
    [[nodiscard]] operator bool() const noexcept { return valid(); }

private:
    PluginLoader() = default;

    /// @brief Load required function from the DLL
    template <typename T>
    [[nodiscard]] T getProc(const char* name) const {
        return reinterpret_cast<T>(GetProcAddress(module_, name));
    }

    HMODULE module_ = nullptr;
    std::filesystem::path path_;

    // Cached plugin info
    const NivePluginInfo* info_ = nullptr;

    // Function pointers
    NivePluginGetInfoFn get_info_fn_ = nullptr;
    NivePluginCanDecodeFn can_decode_fn_ = nullptr;
    NivePluginDecodeFn decode_fn_ = nullptr;
    NivePluginFreeImageFn free_image_fn_ = nullptr;
    NivePluginInitFn init_fn_ = nullptr;
    NivePluginShutdownFn shutdown_fn_ = nullptr;
};

}  // namespace nive::plugin
