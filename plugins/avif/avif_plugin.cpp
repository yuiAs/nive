/// @file avif_plugin.cpp
/// @brief AVIF image decoder plugin using libavif

#include <avif/avif.h>
#include <nive/plugin_api.h>

#include <cstdlib>
#include <cstring>

namespace {

const char* kExtensions[] = {".avif"};

const NivePluginInfo kPluginInfo = {
    .api_version_major = NIVE_PLUGIN_API_VERSION_MAJOR,
    .api_version_minor = NIVE_PLUGIN_API_VERSION_MINOR,
    .name = "AVIF Decoder",
    .version = "1.0.0",
    .author = "yuinshielAs",
    .description = "AVIF image decoder powered by libavif",
    .supported_extensions = kExtensions,
    .extension_count = 1,
};

}  // namespace

extern "C" {

NIVE_PLUGIN_API const NivePluginInfo* nive_plugin_get_info(void) {
    return &kPluginInfo;
}

NIVE_PLUGIN_API int nive_plugin_can_decode(const char* extension) {
    if (!extension) {
        return 0;
    }
    // Case-insensitive comparison for ".avif"
    if (_stricmp(extension, ".avif") == 0) {
        return 1;
    }
    return 0;
}

NIVE_PLUGIN_API NivePluginError nive_plugin_decode(const uint8_t* data, size_t data_size,
                                                    const char* extension,
                                                    NiveDecodedImage* out_image) {
    (void)extension;

    if (!data || data_size == 0 || !out_image) {
        return NIVE_PLUGIN_ERROR_INVALID_DATA;
    }

    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) {
        return NIVE_PLUGIN_ERROR_OUT_OF_MEMORY;
    }

    // Configure decoder
    decoder->maxThreads = 1;
    decoder->strictFlags = AVIF_STRICT_DISABLED;

    avifResult result = avifDecoderSetIOMemory(decoder, data, data_size);
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return NIVE_PLUGIN_ERROR_DECODE_FAILED;
    }

    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return NIVE_PLUGIN_ERROR_DECODE_FAILED;
    }

    result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        return NIVE_PLUGIN_ERROR_DECODE_FAILED;
    }

    // Convert YUV to RGBA8
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, decoder->image);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;

    avifRGBImageAllocatePixels(&rgb);

    result = avifImageYUVToRGB(decoder->image, &rgb);
    if (result != AVIF_RESULT_OK) {
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        return NIVE_PLUGIN_ERROR_DECODE_FAILED;
    }

    // Copy pixels to output buffer (caller frees via nive_plugin_free_image)
    size_t total_size = static_cast<size_t>(rgb.rowBytes) * rgb.height;
    auto* pixels = static_cast<uint8_t*>(std::malloc(total_size));
    if (!pixels) {
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        return NIVE_PLUGIN_ERROR_OUT_OF_MEMORY;
    }

    // Copy row by row in case of padding
    uint32_t row_bytes = rgb.width * 4;  // RGBA8 = 4 bytes per pixel
    for (uint32_t y = 0; y < rgb.height; ++y) {
        std::memcpy(pixels + y * row_bytes, rgb.pixels + y * rgb.rowBytes, row_bytes);
    }

    out_image->pixels = pixels;
    out_image->width = rgb.width;
    out_image->height = rgb.height;
    out_image->format = NIVE_PIXEL_FORMAT_RGBA8;

    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);

    return NIVE_PLUGIN_OK;
}

NIVE_PLUGIN_API void nive_plugin_free_image(NiveDecodedImage* image) {
    if (image && image->pixels) {
        std::free(image->pixels);
        image->pixels = nullptr;
    }
}

NIVE_PLUGIN_API int nive_plugin_has_settings(void) {
    return 0;
}

}  // extern "C"
