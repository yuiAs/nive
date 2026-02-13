/// @file com_ptr.hpp
/// @brief COM smart pointer wrapper
///
/// Uses Microsoft's WRL ComPtr for COM interface management.
/// This provides proper RAII semantics and works seamlessly with
/// Windows SDK macros like IID_PPV_ARGS.

#pragma once

#include <wrl/client.h>

namespace nive {

/// @brief Smart pointer for COM interfaces
///
/// Re-exports Microsoft::WRL::ComPtr for convenience.
/// Provides automatic reference counting via AddRef/Release.
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

}  // namespace nive
