/// @file hash.cpp
/// @brief SHA256 hash implementation using Windows CNG (BCrypt)

#include "hash.hpp"

#include <format>
#include <memory>

// Link with bcrypt.lib (handled in CMakeLists.txt)
#pragma comment(lib, "bcrypt.lib")

namespace nive {

namespace {

/// @brief RAII wrapper for BCrypt algorithm handle
class BcryptAlgorithm {
public:
    BcryptAlgorithm() = default;
    ~BcryptAlgorithm() {
        if (handle_) {
            BCryptCloseAlgorithmProvider(handle_, 0);
        }
    }

    // Non-copyable
    BcryptAlgorithm(const BcryptAlgorithm&) = delete;
    BcryptAlgorithm& operator=(const BcryptAlgorithm&) = delete;

    // Movable
    BcryptAlgorithm(BcryptAlgorithm&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    BcryptAlgorithm& operator=(BcryptAlgorithm&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                BCryptCloseAlgorithmProvider(handle_, 0);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool open(LPCWSTR algorithm_id) {
        NTSTATUS status = BCryptOpenAlgorithmProvider(&handle_, algorithm_id, nullptr, 0);
        return BCRYPT_SUCCESS(status);
    }

    [[nodiscard]] BCRYPT_ALG_HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

/// @brief RAII wrapper for BCrypt hash handle
class BcryptHash {
public:
    BcryptHash() = default;
    ~BcryptHash() {
        if (handle_) {
            BCryptDestroyHash(handle_);
        }
    }

    // Non-copyable, non-movable
    BcryptHash(const BcryptHash&) = delete;
    BcryptHash& operator=(const BcryptHash&) = delete;
    BcryptHash(BcryptHash&&) = delete;
    BcryptHash& operator=(BcryptHash&&) = delete;

    [[nodiscard]] bool create(BCRYPT_ALG_HANDLE algorithm, PUCHAR hash_object,
                              ULONG hash_object_size) {
        NTSTATUS status =
            BCryptCreateHash(algorithm, &handle_, hash_object, hash_object_size, nullptr, 0, 0);
        return BCRYPT_SUCCESS(status);
    }

    [[nodiscard]] bool hash_data(const uint8_t* data, size_t size) {
        NTSTATUS status =
            BCryptHashData(handle_, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
        return BCRYPT_SUCCESS(status);
    }

    [[nodiscard]] bool finish(uint8_t* output, size_t output_size) {
        NTSTATUS status = BCryptFinishHash(handle_, output, static_cast<ULONG>(output_size), 0);
        return BCRYPT_SUCCESS(status);
    }

    [[nodiscard]] BCRYPT_HASH_HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
    BCRYPT_HASH_HANDLE handle_ = nullptr;
};

}  // namespace

std::expected<Sha256Hash, HashError> sha256(std::span<const uint8_t> data) {
    // Open algorithm provider
    BcryptAlgorithm algorithm;
    if (!algorithm.open(BCRYPT_SHA256_ALGORITHM)) {
        return std::unexpected(HashError::InitializationFailed);
    }

    // Get hash object size
    DWORD hash_object_size = 0;
    DWORD result_size = 0;
    NTSTATUS status = BCryptGetProperty(algorithm.get(), BCRYPT_OBJECT_LENGTH,
                                        reinterpret_cast<PUCHAR>(&hash_object_size),
                                        sizeof(hash_object_size), &result_size, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return std::unexpected(HashError::InitializationFailed);
    }

    // Allocate hash object
    auto hash_object = std::make_unique<uint8_t[]>(hash_object_size);

    // Create hash
    BcryptHash hash;
    if (!hash.create(algorithm.get(), hash_object.get(), hash_object_size)) {
        return std::unexpected(HashError::InitializationFailed);
    }

    // Hash the data
    if (!hash.hash_data(data.data(), data.size())) {
        return std::unexpected(HashError::ComputationFailed);
    }

    // Get the result
    Sha256Hash result{};
    if (!hash.finish(result.data(), result.size())) {
        return std::unexpected(HashError::ComputationFailed);
    }

    return result;
}

std::expected<Sha256Hash, HashError> sha256(std::string_view str) {
    return sha256(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size()));
}

std::string hashToHex(const Sha256Hash& hash) {
    std::string result;
    result.reserve(hash.size() * 2);

    for (uint8_t byte : hash) {
        result += std::format("{:02x}", byte);
    }

    return result;
}

std::expected<std::string, HashError> sha256Hex(std::span<const uint8_t> data) {
    auto hash_result = sha256(data);
    if (!hash_result) {
        return std::unexpected(hash_result.error());
    }
    return hashToHex(*hash_result);
}

std::expected<std::string, HashError> sha256Hex(std::string_view str) {
    auto hash_result = sha256(str);
    if (!hash_result) {
        return std::unexpected(hash_result.error());
    }
    return hashToHex(*hash_result);
}

}  // namespace nive
