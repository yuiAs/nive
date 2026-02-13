/// @file plugin_api.h
/// @brief nive Plugin API - C ABI interface for image decoder plugins
///
/// This header defines the C ABI interface that plugins must implement
/// to provide custom image decoding capabilities.
///
/// Plugins are loaded dynamically at runtime and must export the
/// required functions with C linkage.

#ifndef NIVE_PLUGIN_API_H
#define NIVE_PLUGIN_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Plugin API version
#define NIVE_PLUGIN_API_VERSION_MAJOR 1
#define NIVE_PLUGIN_API_VERSION_MINOR 0

// Export macro for plugin functions
#ifdef _WIN32
    #ifdef NIVE_PLUGIN_EXPORT
        #define NIVE_PLUGIN_API __declspec(dllexport)
    #else
        #define NIVE_PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define NIVE_PLUGIN_API __attribute__((visibility("default")))
#endif

// Pixel formats
typedef enum NivePixelFormat {
    NIVE_PIXEL_FORMAT_RGBA8 = 0,  // 4 bytes per pixel, RGBA order
    NIVE_PIXEL_FORMAT_RGB8 = 1,   // 3 bytes per pixel, RGB order
    NIVE_PIXEL_FORMAT_GRAY8 = 2,  // 1 byte per pixel, grayscale
} NivePixelFormat;

// Error codes
typedef enum NivePluginError {
    NIVE_PLUGIN_OK = 0,
    NIVE_PLUGIN_ERROR_UNSUPPORTED_FORMAT = 1,
    NIVE_PLUGIN_ERROR_DECODE_FAILED = 2,
    NIVE_PLUGIN_ERROR_INVALID_DATA = 3,
    NIVE_PLUGIN_ERROR_OUT_OF_MEMORY = 4,
    NIVE_PLUGIN_ERROR_IO = 5,
} NivePluginError;

// Plugin information structure
typedef struct NivePluginInfo {
    uint32_t api_version_major;
    uint32_t api_version_minor;
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    const char** supported_extensions;  // NULL-terminated array
    size_t extension_count;
} NivePluginInfo;

// Decoded image result
typedef struct NiveDecodedImage {
    uint8_t* pixels;  // Pixel data (caller must free with nive_plugin_free_image)
    uint32_t width;
    uint32_t height;
    NivePixelFormat format;
} NiveDecodedImage;

/// Required: Get plugin information
/// @return Pointer to static plugin info structure
typedef const NivePluginInfo* (*NivePluginGetInfoFn)(void);

/// Required: Check if plugin can decode the given format
/// @param extension File extension (e.g., ".webp", lowercase, with dot)
/// @return 1 if supported, 0 if not
typedef int (*NivePluginCanDecodeFn)(const char* extension);

/// Required: Decode image from memory buffer
/// @param data Input image data
/// @param data_size Size of input data in bytes
/// @param extension File extension hint (may be NULL)
/// @param out_image Output image structure (filled on success)
/// @return NIVE_PLUGIN_OK on success, error code on failure
typedef NivePluginError (*NivePluginDecodeFn)(const uint8_t* data, size_t data_size,
                                              const char* extension, NiveDecodedImage* out_image);

/// Required: Free decoded image data
/// @param image Image to free (pixels will be freed, structure is not freed)
typedef void (*NivePluginFreeImageFn)(NiveDecodedImage* image);

/// Optional: Initialize plugin (called once on load)
/// @return NIVE_PLUGIN_OK on success, error code on failure
typedef NivePluginError (*NivePluginInitFn)(void);

/// Optional: Shutdown plugin (called once on unload)
typedef void (*NivePluginShutdownFn)(void);

// Function names for GetProcAddress
#define NIVE_PLUGIN_GET_INFO_NAME "nive_plugin_get_info"
#define NIVE_PLUGIN_CAN_DECODE_NAME "nive_plugin_can_decode"
#define NIVE_PLUGIN_DECODE_NAME "nive_plugin_decode"
#define NIVE_PLUGIN_FREE_IMAGE_NAME "nive_plugin_free_image"
#define NIVE_PLUGIN_INIT_NAME "nive_plugin_init"
#define NIVE_PLUGIN_SHUTDOWN_NAME "nive_plugin_shutdown"

#ifdef __cplusplus
}
#endif

#endif  // NIVE_PLUGIN_API_H
