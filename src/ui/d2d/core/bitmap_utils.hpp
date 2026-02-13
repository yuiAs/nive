/// @file bitmap_utils.hpp
/// @brief Utilities for converting image data to D2D bitmaps

#pragma once

#include <d2d1.h>

#include "core/util/com_ptr.hpp"

namespace nive::image {
class DecodedImage;
}

namespace nive::ui::d2d {

/// @brief Create a D2D bitmap from a DecodedImage (BGRA32, top-down)
/// @param rt Render target to create the bitmap on
/// @param image Source image data
/// @return D2D bitmap, or nullptr on failure
[[nodiscard]] ComPtr<ID2D1Bitmap> createBitmapFromDecodedImage(ID2D1RenderTarget* rt,
                                                               const image::DecodedImage& image);

}  // namespace nive::ui::d2d
