/// @file bitmap_utils.cpp
/// @brief Implementation of D2D bitmap conversion utilities

#include "bitmap_utils.hpp"

#include "core/image/decoded_image.hpp"

namespace nive::ui::d2d {

ComPtr<ID2D1Bitmap> createBitmapFromDecodedImage(ID2D1RenderTarget* rt,
                                                  const image::DecodedImage& image) {
    if (!rt || !image.valid()) {
        return nullptr;
    }

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);

    ComPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = rt->CreateBitmap(D2D1::SizeU(image.width(), image.height()), image.data(),
                                  image.stride(), props, &bitmap);

    if (FAILED(hr)) {
        return nullptr;
    }

    return bitmap;
}

}  // namespace nive::ui::d2d
