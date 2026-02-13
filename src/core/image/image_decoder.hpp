/// @file image_decoder.hpp
/// @brief Abstract image decoder interface
///
/// Defines the interface for image decoders that can be used
/// by the decoder chain.

#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "decoded_image.hpp"

namespace nive::image {

/// @brief Image decoding errors
enum class DecodeError {
    FileNotFound,
    AccessDenied,
    UnsupportedFormat,
    CorruptedData,
    OutOfMemory,
    DecoderNotAvailable,
    InternalError,
};

/// @brief Get string representation of decode error
[[nodiscard]] constexpr std::string_view to_string(DecodeError error) noexcept {
    switch (error) {
    case DecodeError::FileNotFound:
        return "File not found";
    case DecodeError::AccessDenied:
        return "Access denied";
    case DecodeError::UnsupportedFormat:
        return "Unsupported image format";
    case DecodeError::CorruptedData:
        return "Corrupted image data";
    case DecodeError::OutOfMemory:
        return "Out of memory";
    case DecodeError::DecoderNotAvailable:
        return "Decoder not available";
    case DecodeError::InternalError:
        return "Internal decoder error";
    }
    return "Unknown decode error";
}

/// @brief Abstract image decoder interface
///
/// Implementations should be stateless and thread-safe.
class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;

    /// @brief Get decoder name for debugging/logging
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// @brief Get list of supported file extensions
    /// @return Extensions without dot (e.g., "png", "jpg")
    [[nodiscard]] virtual std::span<const std::string_view>
    supportedExtensions() const noexcept = 0;

    /// @brief Check if this decoder can handle the given extension
    /// @param extension File extension without dot (lowercase)
    [[nodiscard]] virtual bool supportsExtension(std::string_view extension) const noexcept = 0;

    /// @brief Check if this decoder can decode the given data
    /// @param data First few bytes of file (for magic number detection)
    /// @return true if decoder can handle this data
    ///
    /// This allows format detection based on file contents rather than extension.
    [[nodiscard]] virtual bool canDecode(std::span<const uint8_t> data) const noexcept = 0;

    /// @brief Get image info without full decode
    /// @param path Path to image file
    /// @return Image info or error
    [[nodiscard]] virtual std::expected<ImageInfo, DecodeError>
    getInfo(const std::filesystem::path& path) const = 0;

    /// @brief Get image info from memory
    /// @param data Image data in memory
    /// @return Image info or error
    [[nodiscard]] virtual std::expected<ImageInfo, DecodeError>
    getInfoFromMemory(std::span<const uint8_t> data) const = 0;

    /// @brief Decode image from file
    /// @param path Path to image file
    /// @param target_format Preferred output format (decoder may ignore)
    /// @return Decoded image or error
    [[nodiscard]] virtual std::expected<DecodedImage, DecodeError>
    decode(const std::filesystem::path& path,
           PixelFormat target_format = PixelFormat::BGRA32) const = 0;

    /// @brief Decode image from memory
    /// @param data Image data in memory
    /// @param target_format Preferred output format (decoder may ignore)
    /// @return Decoded image or error
    [[nodiscard]] virtual std::expected<DecodedImage, DecodeError>
    decodeFromMemory(std::span<const uint8_t> data,
                     PixelFormat target_format = PixelFormat::BGRA32) const = 0;

    /// @brief Decode specific frame from animated image
    /// @param path Path to image file
    /// @param frame_index Frame index (0-based)
    /// @param target_format Preferred output format
    /// @return Decoded frame or error
    [[nodiscard]] virtual std::expected<DecodedImage, DecodeError>
    decodeFrame(const std::filesystem::path& path, uint32_t frame_index,
                PixelFormat target_format = PixelFormat::BGRA32) const = 0;

protected:
    IImageDecoder() = default;
    IImageDecoder(const IImageDecoder&) = default;
    IImageDecoder& operator=(const IImageDecoder&) = default;
    IImageDecoder(IImageDecoder&&) = default;
    IImageDecoder& operator=(IImageDecoder&&) = default;
};

}  // namespace nive::image
