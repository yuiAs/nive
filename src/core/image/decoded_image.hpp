/// @file decoded_image.hpp
/// @brief Decoded image data container
///
/// Holds pixel data after decoding from various image formats.

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace nive::image {

/// @brief Pixel format enumeration
enum class PixelFormat {
    Unknown,
    BGRA32,  // 32-bit BGRA (Windows native)
    RGBA32,  // 32-bit RGBA
    BGR24,   // 24-bit BGR
    RGB24,   // 24-bit RGB
    Gray8,   // 8-bit grayscale
    Gray16,  // 16-bit grayscale
};

/// @brief Get bytes per pixel for a pixel format
[[nodiscard]] constexpr uint32_t bytesPerPixel(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::BGRA32:
    case PixelFormat::RGBA32:
        return 4;
    case PixelFormat::BGR24:
    case PixelFormat::RGB24:
        return 3;
    case PixelFormat::Gray8:
        return 1;
    case PixelFormat::Gray16:
        return 2;
    default:
        return 0;
    }
}

/// @brief Check if pixel format has alpha channel
[[nodiscard]] constexpr bool hasAlpha(PixelFormat format) noexcept {
    return format == PixelFormat::BGRA32 || format == PixelFormat::RGBA32;
}

/// @brief Decoded image data
///
/// Owns the pixel data buffer and provides access to image properties.
/// Uses BGRA32 as the primary format for Windows compatibility.
class DecodedImage {
public:
    /// @brief Construct empty image
    DecodedImage() = default;

    /// @brief Construct image with dimensions and format
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @param format Pixel format
    DecodedImage(uint32_t width, uint32_t height, PixelFormat format)
        : width_(width), height_(height), format_(format), stride_(calculate_stride(width, format)),
          pixels_(static_cast<size_t>(stride_) * height) {}

    /// @brief Construct image with existing pixel data (takes ownership)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// @param format Pixel format
    /// @param stride Row stride in bytes
    /// @param pixels Pixel data (moved)
    DecodedImage(uint32_t width, uint32_t height, PixelFormat format, uint32_t stride,
                 std::vector<uint8_t> pixels)
        : width_(width), height_(height), format_(format), stride_(stride),
          pixels_(std::move(pixels)) {}

    // Move-only type
    DecodedImage(const DecodedImage&) = delete;
    DecodedImage& operator=(const DecodedImage&) = delete;
    DecodedImage(DecodedImage&&) noexcept = default;
    DecodedImage& operator=(DecodedImage&&) noexcept = default;

    ~DecodedImage() = default;

    /// @brief Check if image is valid (has data)
    [[nodiscard]] bool valid() const noexcept {
        return width_ > 0 && height_ > 0 && !pixels_.empty();
    }

    /// @brief Check if image is valid
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    // Accessors
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] uint32_t stride() const noexcept { return stride_; }

    /// @brief Get total size of pixel data in bytes
    [[nodiscard]] size_t sizeBytes() const noexcept { return pixels_.size(); }

    /// @brief Get read-only access to pixel data
    [[nodiscard]] std::span<const uint8_t> pixels() const noexcept { return pixels_; }

    /// @brief Get mutable access to pixel data
    [[nodiscard]] std::span<uint8_t> pixels() noexcept { return pixels_; }

    /// @brief Get pointer to pixel data
    [[nodiscard]] const uint8_t* data() const noexcept { return pixels_.data(); }

    /// @brief Get mutable pointer to pixel data
    [[nodiscard]] uint8_t* data() noexcept { return pixels_.data(); }

    /// @brief Get pointer to specific row
    /// @param row Row index (0-based)
    [[nodiscard]] const uint8_t* row(uint32_t row) const noexcept {
        return pixels_.data() + static_cast<size_t>(row) * stride_;
    }

    /// @brief Get mutable pointer to specific row
    /// @param row Row index (0-based)
    [[nodiscard]] uint8_t* row(uint32_t row) noexcept {
        return pixels_.data() + static_cast<size_t>(row) * stride_;
    }

    /// @brief Release ownership of pixel data
    [[nodiscard]] std::vector<uint8_t> release() noexcept {
        width_ = 0;
        height_ = 0;
        format_ = PixelFormat::Unknown;
        stride_ = 0;
        return std::move(pixels_);
    }

private:
    /// @brief Calculate stride (row pitch) with 4-byte alignment
    [[nodiscard]] static uint32_t calculate_stride(uint32_t width, PixelFormat format) noexcept {
        uint32_t bpp = bytesPerPixel(format);
        uint32_t row_bytes = width * bpp;
        // Align to 4 bytes (DWORD alignment for Windows GDI)
        return (row_bytes + 3) & ~3u;
    }

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    PixelFormat format_ = PixelFormat::Unknown;
    uint32_t stride_ = 0;
    std::vector<uint8_t> pixels_;
};

/// @brief Image metadata (dimensions, format info)
struct ImageInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    uint32_t frame_count = 1;  // For animated images
    bool has_alpha = false;
    uint32_t dpi_x = 96;
    uint32_t dpi_y = 96;
};

}  // namespace nive::image
