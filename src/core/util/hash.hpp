/// @file hash.hpp
/// @brief Cryptographic hash utilities using Windows CNG (BCrypt)
///
/// Provides SHA256 hashing for cache key generation.

#pragma once

#include <Windows.h>

#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include <bcrypt.h>

namespace nive {

/// @brief SHA256 hash result (32 bytes)
using Sha256Hash = std::array<uint8_t, 32>;

/// @brief Hash computation errors
enum class HashError {
    InitializationFailed,
    ComputationFailed,
};

/// @brief Get string representation of hash error
[[nodiscard]] constexpr std::string_view to_string(HashError error) noexcept {
    switch (error) {
    case HashError::InitializationFailed:
        return "Hash initialization failed";
    case HashError::ComputationFailed:
        return "Hash computation failed";
    }
    return "Unknown hash error";
}

/// @brief Compute SHA256 hash of data
/// @param data Data to hash
/// @return Hash result or error
[[nodiscard]] std::expected<Sha256Hash, HashError> sha256(std::span<const uint8_t> data);

/// @brief Compute SHA256 hash of string
/// @param str String to hash
/// @return Hash result or error
[[nodiscard]] std::expected<Sha256Hash, HashError> sha256(std::string_view str);

/// @brief Convert hash to hexadecimal string
/// @param hash Hash bytes
/// @return 64-character lowercase hex string
[[nodiscard]] std::string hashToHex(const Sha256Hash& hash);

/// @brief Compute SHA256 hash and return as hex string
/// @param data Data to hash
/// @return Hex string or error
[[nodiscard]] std::expected<std::string, HashError> sha256Hex(std::span<const uint8_t> data);

/// @brief Compute SHA256 hash of string and return as hex string
/// @param str String to hash
/// @return Hex string or error
[[nodiscard]] std::expected<std::string, HashError> sha256Hex(std::string_view str);

}  // namespace nive
