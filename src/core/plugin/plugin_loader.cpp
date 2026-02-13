/// @file plugin_loader.cpp
/// @brief Dynamic library loading implementation

#include "plugin_loader.hpp"

namespace nive::plugin {

std::expected<PluginLoader, LoadError> PluginLoader::load(const std::filesystem::path& path) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return std::unexpected(LoadError::FileNotFound);
    }

    PluginLoader loader;
    loader.path_ = path;

    // Load the DLL
    loader.module_ = LoadLibraryW(path.c_str());
    if (!loader.module_) {
        return std::unexpected(LoadError::LoadFailed);
    }

    // Get required function pointers
    loader.get_info_fn_ = loader.getProc<NivePluginGetInfoFn>(NIVE_PLUGIN_GET_INFO_NAME);
    if (!loader.get_info_fn_) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::MissingExport);
    }

    loader.can_decode_fn_ = loader.getProc<NivePluginCanDecodeFn>(NIVE_PLUGIN_CAN_DECODE_NAME);
    if (!loader.can_decode_fn_) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::MissingExport);
    }

    loader.decode_fn_ = loader.getProc<NivePluginDecodeFn>(NIVE_PLUGIN_DECODE_NAME);
    if (!loader.decode_fn_) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::MissingExport);
    }

    loader.free_image_fn_ = loader.getProc<NivePluginFreeImageFn>(NIVE_PLUGIN_FREE_IMAGE_NAME);
    if (!loader.free_image_fn_) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::MissingExport);
    }

    // Get optional function pointers
    loader.init_fn_ = loader.getProc<NivePluginInitFn>(NIVE_PLUGIN_INIT_NAME);
    loader.shutdown_fn_ = loader.getProc<NivePluginShutdownFn>(NIVE_PLUGIN_SHUTDOWN_NAME);

    // Get plugin info
    loader.info_ = loader.get_info_fn_();
    if (!loader.info_) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::MissingExport);
    }

    // Check API version compatibility
    if (loader.info_->api_version_major != NIVE_PLUGIN_API_VERSION_MAJOR) {
        FreeLibrary(loader.module_);
        loader.module_ = nullptr;
        return std::unexpected(LoadError::VersionMismatch);
    }

    // Initialize plugin if init function exists
    if (loader.init_fn_) {
        NivePluginError result = loader.init_fn_();
        if (result != NIVE_PLUGIN_OK) {
            FreeLibrary(loader.module_);
            loader.module_ = nullptr;
            return std::unexpected(LoadError::InitFailed);
        }
    }

    return loader;
}

PluginLoader::~PluginLoader() {
    if (module_) {
        // Call shutdown if available
        if (shutdown_fn_) {
            shutdown_fn_();
        }
        FreeLibrary(module_);
    }
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : module_(other.module_), path_(std::move(other.path_)), info_(other.info_),
      get_info_fn_(other.get_info_fn_), can_decode_fn_(other.can_decode_fn_),
      decode_fn_(other.decode_fn_), free_image_fn_(other.free_image_fn_), init_fn_(other.init_fn_),
      shutdown_fn_(other.shutdown_fn_) {
    other.module_ = nullptr;
    other.info_ = nullptr;
    other.get_info_fn_ = nullptr;
    other.can_decode_fn_ = nullptr;
    other.decode_fn_ = nullptr;
    other.free_image_fn_ = nullptr;
    other.init_fn_ = nullptr;
    other.shutdown_fn_ = nullptr;
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if (this != &other) {
        // Clean up current
        if (module_) {
            if (shutdown_fn_) {
                shutdown_fn_();
            }
            FreeLibrary(module_);
        }

        // Move from other
        module_ = other.module_;
        path_ = std::move(other.path_);
        info_ = other.info_;
        get_info_fn_ = other.get_info_fn_;
        can_decode_fn_ = other.can_decode_fn_;
        decode_fn_ = other.decode_fn_;
        free_image_fn_ = other.free_image_fn_;
        init_fn_ = other.init_fn_;
        shutdown_fn_ = other.shutdown_fn_;

        // Clear other
        other.module_ = nullptr;
        other.info_ = nullptr;
        other.get_info_fn_ = nullptr;
        other.can_decode_fn_ = nullptr;
        other.decode_fn_ = nullptr;
        other.free_image_fn_ = nullptr;
        other.init_fn_ = nullptr;
        other.shutdown_fn_ = nullptr;
    }
    return *this;
}

bool PluginLoader::canDecode(const std::string& extension) const {
    if (!can_decode_fn_) {
        return false;
    }
    return can_decode_fn_(extension.c_str()) != 0;
}

std::expected<NiveDecodedImage, NivePluginError>
PluginLoader::decode(const uint8_t* data, size_t size, const std::string& extension) const {
    if (!decode_fn_) {
        return std::unexpected(NIVE_PLUGIN_ERROR_DECODE_FAILED);
    }

    NiveDecodedImage image = {};
    NivePluginError result =
        decode_fn_(data, size, extension.empty() ? nullptr : extension.c_str(), &image);

    if (result != NIVE_PLUGIN_OK) {
        return std::unexpected(result);
    }

    return image;
}

void PluginLoader::freeImage(NiveDecodedImage& image) const {
    if (free_image_fn_ && image.pixels) {
        free_image_fn_(&image);
        image.pixels = nullptr;
    }
}

}  // namespace nive::plugin
