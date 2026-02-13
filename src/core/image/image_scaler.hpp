/// @file image_scaler.hpp
/// @brief Image scaling using WIC
///
/// Provides high-quality image scaling for thumbnail generation
/// and display purposes.

#pragma once

#include <expected>

#include "decoded_image.hpp"
#include "image_decoder.hpp"

namespace nive::image {

/// @brief Scaling algorithm
enum class ScaleMode {
    NearestNeighbor,   // Fastest, pixelated
    Linear,            // Bilinear interpolation
    Cubic,             // Bicubic interpolation (default)
    HighQualityCubic,  // Best quality, slowest
    Fant,              // Good for downscaling
};

/// @brief Fit mode for scaling to target dimensions
enum class FitMode {
    Fill,       // Fill target exactly (may distort)
    Contain,    // Fit inside target (preserve aspect, may have padding)
    Cover,      // Cover target (preserve aspect, may crop)
    ScaleDown,  // Only scale down, never up
};

/// @brief Scaling options
struct ScaleOptions {
    ScaleMode mode = ScaleMode::Cubic;
    FitMode fit = FitMode::Contain;
    PixelFormat output_format = PixelFormat::BGRA32;
};

/// @brief Scale an image to target dimensions
/// @param source Source image
/// @param target_width Target width in pixels
/// @param target_height Target height in pixels
/// @param options Scaling options
/// @return Scaled image or error
[[nodiscard]] std::expected<DecodedImage, DecodeError> scaleImage(const DecodedImage& source,
                                                                  uint32_t target_width,
                                                                  uint32_t target_height,
                                                                  const ScaleOptions& options = {});

/// @brief Calculate scaled dimensions preserving aspect ratio
/// @param source_width Source width
/// @param source_height Source height
/// @param max_width Maximum target width
/// @param max_height Maximum target height
/// @param fit Fit mode
/// @return Pair of (width, height)
[[nodiscard]] std::pair<uint32_t, uint32_t>
calculateScaledDimensions(uint32_t source_width, uint32_t source_height, uint32_t max_width,
                          uint32_t max_height, FitMode fit = FitMode::Contain);

/// @brief Generate thumbnail from decoded image
/// @param source Source image
/// @param max_size Maximum dimension (width or height)
/// @param options Scaling options (fit mode is always Contain)
/// @return Thumbnail image or error
[[nodiscard]] std::expected<DecodedImage, DecodeError>
generateThumbnail(const DecodedImage& source, uint32_t max_size, const ScaleOptions& options = {});

}  // namespace nive::image
