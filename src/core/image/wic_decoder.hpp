/// @file wic_decoder.hpp
/// @brief Windows Imaging Component (WIC) based image decoder
///
/// Uses WIC to decode common image formats (PNG, JPEG, BMP, GIF, TIFF, etc.)

#pragma once

#include "image_decoder.hpp"

namespace nive::image {

/// @brief WIC-based image decoder
///
/// Supports all formats that WIC supports natively:
/// - PNG, JPEG, BMP, GIF, TIFF, ICO, WDP/HDP (HD Photo)
/// - Additional codecs may be installed system-wide
///
/// Thread-safe: yes (uses per-call COM objects)
class WicDecoder final : public IImageDecoder {
public:
    WicDecoder();
    ~WicDecoder() override;

    // Non-copyable, non-movable (holds COM factory)
    WicDecoder(const WicDecoder&) = delete;
    WicDecoder& operator=(const WicDecoder&) = delete;
    WicDecoder(WicDecoder&&) = delete;
    WicDecoder& operator=(WicDecoder&&) = delete;

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] std::span<const std::string_view> supportedExtensions() const noexcept override;

    [[nodiscard]] bool supportsExtension(std::string_view extension) const noexcept override;

    [[nodiscard]] bool canDecode(std::span<const uint8_t> data) const noexcept override;

    [[nodiscard]] std::expected<ImageInfo, DecodeError>
    getInfo(const std::filesystem::path& path) const override;

    [[nodiscard]] std::expected<ImageInfo, DecodeError>
    getInfoFromMemory(std::span<const uint8_t> data) const override;

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decode(const std::filesystem::path& path,
           PixelFormat target_format = PixelFormat::BGRA32) const override;

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decodeFromMemory(std::span<const uint8_t> data,
                     PixelFormat target_format = PixelFormat::BGRA32) const override;

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decodeFrame(const std::filesystem::path& path, uint32_t frame_index,
                PixelFormat target_format = PixelFormat::BGRA32) const override;

    /// @brief Check if WIC is available on this system
    [[nodiscard]] bool isAvailable() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nive::image
