/// @file d2d_factory.hpp
/// @brief Singleton management for D2D1 and DirectWrite factories

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include "core/util/com_ptr.hpp"

namespace nive::ui::d2d {

/// @brief Singleton accessor for Direct2D and DirectWrite factories
///
/// The factories are created lazily on first access and destroyed when
/// the application terminates. Thread-safe via std::call_once.
class D2DFactory {
public:
    /// @brief Get the singleton instance
    [[nodiscard]] static D2DFactory& instance();

    /// @brief Get the Direct2D factory
    [[nodiscard]] ID2D1Factory* d2dFactory() const noexcept { return d2d_factory_.Get(); }

    /// @brief Get the DirectWrite factory
    [[nodiscard]] IDWriteFactory* dwriteFactory() const noexcept { return dwrite_factory_.Get(); }

    /// @brief Check if factories are initialized successfully
    [[nodiscard]] bool isValid() const noexcept { return d2d_factory_ && dwrite_factory_; }

    D2DFactory(const D2DFactory&) = delete;
    D2DFactory& operator=(const D2DFactory&) = delete;

private:
    D2DFactory();
    ~D2DFactory() = default;

    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<IDWriteFactory> dwrite_factory_;
};

}  // namespace nive::ui::d2d
