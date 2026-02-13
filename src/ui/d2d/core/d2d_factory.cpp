/// @file d2d_factory.cpp
/// @brief Implementation of D2D factory singleton

#include "d2d_factory.hpp"

#include <mutex>

#include "core/util/logger.hpp"

namespace nive::ui::d2d {

D2DFactory& D2DFactory::instance() {
    static D2DFactory instance;
    return instance;
}

D2DFactory::D2DFactory() {
    // Create D2D1 factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d_factory_));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D2D1 factory: 0x{:08X}", static_cast<unsigned>(hr));
        return;
    }

    // Create DirectWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DirectWrite factory: 0x{:08X}", static_cast<unsigned>(hr));
        d2d_factory_.Reset();
        return;
    }

    LOG_DEBUG("D2D factories initialized successfully");
}

}  // namespace nive::ui::d2d
