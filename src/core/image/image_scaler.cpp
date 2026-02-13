/// @file image_scaler.cpp
/// @brief Image scaling implementation using WIC

#include "image_scaler.hpp"
#include <Windows.h>

#include <wincodec.h>

#include <algorithm>
#include <cmath>

#include "../util/com_ptr.hpp"

#pragma comment(lib, "windowscodecs.lib")

namespace nive::image {

namespace {

/// @brief Convert ScaleMode to WIC interpolation mode
WICBitmapInterpolationMode to_wic_interpolation(ScaleMode mode) {
    switch (mode) {
    case ScaleMode::NearestNeighbor:
        return WICBitmapInterpolationModeNearestNeighbor;
    case ScaleMode::Linear:
        return WICBitmapInterpolationModeLinear;
    case ScaleMode::Cubic:
        return WICBitmapInterpolationModeCubic;
    case ScaleMode::HighQualityCubic:
        return WICBitmapInterpolationModeHighQualityCubic;
    case ScaleMode::Fant:
        return WICBitmapInterpolationModeFant;
    default:
        return WICBitmapInterpolationModeCubic;
    }
}

/// @brief Convert PixelFormat to WIC format GUID
const GUID& pixel_format_to_wic_guid(PixelFormat format) {
    switch (format) {
    case PixelFormat::BGRA32:
        return GUID_WICPixelFormat32bppBGRA;
    case PixelFormat::RGBA32:
        return GUID_WICPixelFormat32bppRGBA;
    case PixelFormat::BGR24:
        return GUID_WICPixelFormat24bppBGR;
    case PixelFormat::RGB24:
        return GUID_WICPixelFormat24bppRGB;
    case PixelFormat::Gray8:
        return GUID_WICPixelFormat8bppGray;
    case PixelFormat::Gray16:
        return GUID_WICPixelFormat16bppGray;
    default:
        return GUID_WICPixelFormat32bppBGRA;
    }
}

/// @brief Get WIC factory (lazily initialized)
IWICImagingFactory* get_wic_factory() {
    static ComPtr<IWICImagingFactory> factory = []() {
        ComPtr<IWICImagingFactory> f;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
        return f;
    }();
    return factory.Get();
}

}  // namespace

std::pair<uint32_t, uint32_t> calculateScaledDimensions(uint32_t source_width,
                                                        uint32_t source_height, uint32_t max_width,
                                                        uint32_t max_height, FitMode fit) {
    if (source_width == 0 || source_height == 0) {
        return {0, 0};
    }
    if (max_width == 0 || max_height == 0) {
        return {source_width, source_height};
    }

    double source_aspect = static_cast<double>(source_width) / source_height;
    double target_aspect = static_cast<double>(max_width) / max_height;

    uint32_t result_width = max_width;
    uint32_t result_height = max_height;

    switch (fit) {
    case FitMode::Fill:
        // Use target dimensions exactly
        break;

    case FitMode::Contain:
        // Fit inside target, preserving aspect ratio
        if (source_aspect > target_aspect) {
            // Width-constrained
            result_width = max_width;
            result_height = static_cast<uint32_t>(std::round(max_width / source_aspect));
        } else {
            // Height-constrained
            result_height = max_height;
            result_width = static_cast<uint32_t>(std::round(max_height * source_aspect));
        }
        break;

    case FitMode::Cover:
        // Cover target, preserving aspect ratio (may exceed bounds)
        if (source_aspect > target_aspect) {
            // Height-constrained
            result_height = max_height;
            result_width = static_cast<uint32_t>(std::round(max_height * source_aspect));
        } else {
            // Width-constrained
            result_width = max_width;
            result_height = static_cast<uint32_t>(std::round(max_width / source_aspect));
        }
        break;

    case FitMode::ScaleDown:
        // Only scale down, never up
        if (source_width <= max_width && source_height <= max_height) {
            return {source_width, source_height};
        }
        // Apply Contain logic for downscaling
        if (source_aspect > target_aspect) {
            result_width = max_width;
            result_height = static_cast<uint32_t>(std::round(max_width / source_aspect));
        } else {
            result_height = max_height;
            result_width = static_cast<uint32_t>(std::round(max_height * source_aspect));
        }
        break;
    }

    // Ensure at least 1x1
    result_width = std::max(result_width, 1u);
    result_height = std::max(result_height, 1u);

    return {result_width, result_height};
}

std::expected<DecodedImage, DecodeError> scaleImage(const DecodedImage& source,
                                                    uint32_t target_width, uint32_t target_height,
                                                    const ScaleOptions& options) {
    if (!source.valid()) {
        return std::unexpected(DecodeError::CorruptedData);
    }

    if (target_width == 0 || target_height == 0) {
        return std::unexpected(DecodeError::InternalError);
    }

    auto* factory = get_wic_factory();
    if (!factory) {
        return std::unexpected(DecodeError::DecoderNotAvailable);
    }

    // Calculate actual dimensions based on fit mode
    auto [actual_width, actual_height] = calculateScaledDimensions(
        source.width(), source.height(), target_width, target_height, options.fit);

    // If no scaling needed, just convert format if necessary
    if (actual_width == source.width() && actual_height == source.height() &&
        options.output_format == source.format()) {
        // Return a copy
        std::vector<uint8_t> pixels(source.pixels().begin(), source.pixels().end());
        return DecodedImage(source.width(), source.height(), source.format(), source.stride(),
                            std::move(pixels));
    }

    // Create bitmap from source data
    ComPtr<IWICBitmap> bitmap;
    HRESULT hr = factory->CreateBitmapFromMemory(
        source.width(), source.height(), pixel_format_to_wic_guid(source.format()), source.stride(),
        static_cast<UINT>(source.sizeBytes()), const_cast<BYTE*>(source.data()), &bitmap);
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    // Create scaler
    ComPtr<IWICBitmapScaler> scaler;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    hr = scaler->Initialize(bitmap.Get(), actual_width, actual_height,
                            to_wic_interpolation(options.mode));
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    // Convert to target format if needed
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    hr = converter->Initialize(scaler.Get(), pixel_format_to_wic_guid(options.output_format),
                               WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    // Allocate output buffer
    uint32_t bpp = bytesPerPixel(options.output_format);
    uint32_t stride = (actual_width * bpp + 3) & ~3u;

    std::vector<uint8_t> pixels;
    try {
        pixels.resize(static_cast<size_t>(stride) * actual_height);
    } catch (const std::bad_alloc&) {
        return std::unexpected(DecodeError::OutOfMemory);
    }

    // Copy pixels
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        return std::unexpected(DecodeError::InternalError);
    }

    return DecodedImage(actual_width, actual_height, options.output_format, stride,
                        std::move(pixels));
}

std::expected<DecodedImage, DecodeError>
generateThumbnail(const DecodedImage& source, uint32_t max_size, const ScaleOptions& options) {
    ScaleOptions thumb_options = options;
    thumb_options.fit = FitMode::Contain;

    // Use Fant for better quality when downscaling
    if (thumb_options.mode == ScaleMode::Cubic &&
        (source.width() > max_size * 2 || source.height() > max_size * 2)) {
        thumb_options.mode = ScaleMode::Fant;
    }

    return scaleImage(source, max_size, max_size, thumb_options);
}

}  // namespace nive::image
