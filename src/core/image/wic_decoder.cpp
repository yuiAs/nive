/// @file wic_decoder.cpp
/// @brief WIC decoder implementation

#include "wic_decoder.hpp"
#include <Windows.h>

#include <wincodec.h>

#include <algorithm>
#include <array>

#include "../util/com_ptr.hpp"

#pragma comment(lib, "windowscodecs.lib")

namespace nive::image {

namespace {

// Supported file extensions
constexpr std::array<std::string_view, 10> SUPPORTED_EXTENSIONS = {
    "png", "jpg", "jpeg", "bmp", "gif", "tiff", "tif", "ico", "wdp", "hdp"};

constexpr uint8_t PNG_MAGIC[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr uint8_t JPEG_MAGIC[] = {0xFF, 0xD8, 0xFF};
constexpr uint8_t BMP_MAGIC[] = {0x42, 0x4D};                            // "BM"
constexpr uint8_t GIF87_MAGIC[] = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};  // "GIF87a"
constexpr uint8_t GIF89_MAGIC[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};  // "GIF89a"
constexpr uint8_t TIFF_LE_MAGIC[] = {0x49, 0x49, 0x2A, 0x00};            // Little-endian
constexpr uint8_t TIFF_BE_MAGIC[] = {0x4D, 0x4D, 0x00, 0x2A};            // Big-endian

/// @brief Check if data starts with given signature
bool matches_signature(std::span<const uint8_t> data, std::span<const uint8_t> signature,
                       size_t offset = 0) {
    if (data.size() < offset + signature.size()) {
        return false;
    }
    return std::equal(signature.begin(), signature.end(), data.begin() + offset);
}

/// @brief Convert WIC pixel format GUID to our PixelFormat
PixelFormat wic_format_to_pixel_format(const GUID& wic_format) {
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat32bppBGRA)) {
        return PixelFormat::BGRA32;
    }
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat32bppRGBA)) {
        return PixelFormat::RGBA32;
    }
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat24bppBGR)) {
        return PixelFormat::BGR24;
    }
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat24bppRGB)) {
        return PixelFormat::RGB24;
    }
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat8bppGray)) {
        return PixelFormat::Gray8;
    }
    if (IsEqualGUID(wic_format, GUID_WICPixelFormat16bppGray)) {
        return PixelFormat::Gray16;
    }
    return PixelFormat::Unknown;
}

/// @brief Convert our PixelFormat to WIC pixel format GUID
const GUID& pixel_format_to_wic(PixelFormat format) {
    switch (format) {
    case PixelFormat::BGRA32:
        return GUID_WICPixelFormat32bppBGRA;
    case PixelFormat::RGBA32:
        return GUID_WICPixelFormat32bppRGBA;
    case PixelFormat::BGR24:
        return GUID_WICPixelFormat24bppBGR;
    case PixelFormat::RGB24:
        return GUID_WICPixelFormat24bppRGB;
    case PixelFormat::Gray8:
        return GUID_WICPixelFormat8bppGray;
    case PixelFormat::Gray16:
        return GUID_WICPixelFormat16bppGray;
    default:
        return GUID_WICPixelFormat32bppBGRA;
    }
}

/// @brief Convert HRESULT to DecodeError
DecodeError hresult_to_decode_error(HRESULT hr) {
    switch (hr) {
    case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
    case HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND):
        return DecodeError::FileNotFound;
    case HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED):
        return DecodeError::AccessDenied;
    case WINCODEC_ERR_COMPONENTNOTFOUND:
    case WINCODEC_ERR_UNKNOWNIMAGEFORMAT:
        return DecodeError::UnsupportedFormat;
    case WINCODEC_ERR_BADIMAGE:
    case WINCODEC_ERR_BADHEADER:
    case WINCODEC_ERR_FRAMEMISSING:
        return DecodeError::CorruptedData;
    case E_OUTOFMEMORY:
        return DecodeError::OutOfMemory;
    default:
        return DecodeError::InternalError;
    }
}

}  // namespace

/// @brief WIC decoder implementation
class WicDecoder::Impl {
public:
    Impl() {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory_));
        available_ = SUCCEEDED(hr);
    }

    [[nodiscard]] bool isAvailable() const noexcept { return available_; }

    [[nodiscard]] std::expected<ImageInfo, DecodeError>
    getInfo(const std::filesystem::path& path) const {
        if (!available_) {
            return std::unexpected(DecodeError::DecoderNotAvailable);
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                         WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        return get_info_from_decoder(decoder.Get());
    }

    [[nodiscard]] std::expected<ImageInfo, DecodeError>
    getInfoFromMemory(std::span<const uint8_t> data) const {
        if (!available_) {
            return std::unexpected(DecodeError::DecoderNotAvailable);
        }

        auto stream = create_stream_from_memory(data);
        if (!stream) {
            return std::unexpected(DecodeError::InternalError);
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromStream(stream.Get(), nullptr,
                                                       WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        return get_info_from_decoder(decoder.Get());
    }

    [[nodiscard]] std::expected<DecodedImage, DecodeError> decode(const std::filesystem::path& path,
                                                                  PixelFormat target_format) const {
        return decode_frame_impl(path, 0, target_format);
    }

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decodeFromMemory(std::span<const uint8_t> data, PixelFormat target_format) const {
        if (!available_) {
            return std::unexpected(DecodeError::DecoderNotAvailable);
        }

        auto stream = create_stream_from_memory(data);
        if (!stream) {
            return std::unexpected(DecodeError::InternalError);
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromStream(stream.Get(), nullptr,
                                                       WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        return decode_frame_from_decoder(decoder.Get(), 0, target_format);
    }

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decodeFrame(const std::filesystem::path& path, uint32_t frame_index,
                PixelFormat target_format) const {
        return decode_frame_impl(path, frame_index, target_format);
    }

private:
    [[nodiscard]] std::expected<ImageInfo, DecodeError>
    get_info_from_decoder(IWICBitmapDecoder* decoder) const {
        ComPtr<IWICBitmapFrameDecode> frame;
        HRESULT hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        UINT width = 0, height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        WICPixelFormatGUID pixel_format;
        hr = frame->GetPixelFormat(&pixel_format);
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        UINT frame_count = 0;
        decoder->GetFrameCount(&frame_count);

        double dpi_x = 96.0, dpi_y = 96.0;
        frame->GetResolution(&dpi_x, &dpi_y);

        // Check for alpha
        ComPtr<IWICComponentInfo> comp_info;
        factory_->CreateComponentInfo(pixel_format, &comp_info);

        ComPtr<IWICPixelFormatInfo> format_info;
        bool has_alpha_channel = false;
        if (comp_info && SUCCEEDED(comp_info.As(&format_info))) {
            UINT channel_count = 0;
            format_info->GetChannelCount(&channel_count);
            // Most formats with alpha have 4 channels (RGBA/BGRA)
            has_alpha_channel = (channel_count == 4 || channel_count == 2);
        }

        return ImageInfo{
            .width = width,
            .height = height,
            .format = wic_format_to_pixel_format(pixel_format),
            .frame_count = frame_count > 0 ? frame_count : 1,
            .has_alpha = has_alpha_channel,
            .dpi_x = static_cast<uint32_t>(dpi_x),
            .dpi_y = static_cast<uint32_t>(dpi_y),
        };
    }

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decode_frame_impl(const std::filesystem::path& path, uint32_t frame_index,
                      PixelFormat target_format) const {
        if (!available_) {
            return std::unexpected(DecodeError::DecoderNotAvailable);
        }

        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                         WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        return decode_frame_from_decoder(decoder.Get(), frame_index, target_format);
    }

    [[nodiscard]] std::expected<DecodedImage, DecodeError>
    decode_frame_from_decoder(IWICBitmapDecoder* decoder, uint32_t frame_index,
                              PixelFormat target_format) const {
        ComPtr<IWICBitmapFrameDecode> frame;
        HRESULT hr = decoder->GetFrame(frame_index, &frame);
        if (FAILED(hr)) {
            return std::unexpected(hresult_to_decode_error(hr));
        }

        UINT width = 0, height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        // Convert to target format
        const GUID& target_wic_format = pixel_format_to_wic(target_format);

        ComPtr<IWICFormatConverter> converter;
        hr = factory_->CreateFormatConverter(&converter);
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        hr = converter->Initialize(frame.Get(), target_wic_format, WICBitmapDitherTypeNone, nullptr,
                                   0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        // Calculate stride with 4-byte alignment
        uint32_t bpp = bytesPerPixel(target_format);
        uint32_t stride = (width * bpp + 3) & ~3u;

        // Allocate pixel buffer
        std::vector<uint8_t> pixels;
        try {
            pixels.resize(static_cast<size_t>(stride) * height);
        } catch (const std::bad_alloc&) {
            return std::unexpected(DecodeError::OutOfMemory);
        }

        // Copy pixels
        hr = converter->CopyPixels(nullptr,  // Entire bitmap
                                   stride, static_cast<UINT>(pixels.size()), pixels.data());
        if (FAILED(hr)) {
            return std::unexpected(DecodeError::InternalError);
        }

        return DecodedImage(width, height, target_format, stride, std::move(pixels));
    }

    [[nodiscard]] ComPtr<IStream> create_stream_from_memory(std::span<const uint8_t> data) const {
        ComPtr<IStream> stream;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        if (FAILED(hr)) {
            return nullptr;
        }

        ULONG written = 0;
        hr = stream->Write(data.data(), static_cast<ULONG>(data.size()), &written);
        if (FAILED(hr) || written != data.size()) {
            return nullptr;
        }

        // Seek back to beginning
        LARGE_INTEGER zero = {};
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);

        return stream;
    }

    ComPtr<IWICImagingFactory> factory_;
    bool available_ = false;
};

// Public interface implementation

WicDecoder::WicDecoder() : impl_(std::make_unique<Impl>()) {
}

WicDecoder::~WicDecoder() = default;

std::string_view WicDecoder::name() const noexcept {
    return "WIC";
}

std::span<const std::string_view> WicDecoder::supportedExtensions() const noexcept {
    return SUPPORTED_EXTENSIONS;
}

bool WicDecoder::supportsExtension(std::string_view extension) const noexcept {
    return std::ranges::any_of(SUPPORTED_EXTENSIONS, [extension](std::string_view ext) {
        return std::ranges::equal(ext, extension, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    });
}

bool WicDecoder::canDecode(std::span<const uint8_t> data) const noexcept {
    if (data.size() < 8) {
        return false;
    }

    // Check magic numbers
    if (matches_signature(data, PNG_MAGIC))
        return true;
    if (matches_signature(data, JPEG_MAGIC))
        return true;
    if (matches_signature(data, BMP_MAGIC))
        return true;
    if (matches_signature(data, GIF87_MAGIC))
        return true;
    if (matches_signature(data, GIF89_MAGIC))
        return true;
    if (matches_signature(data, TIFF_LE_MAGIC))
        return true;
    if (matches_signature(data, TIFF_BE_MAGIC))
        return true;

    return false;
}

std::expected<ImageInfo, DecodeError> WicDecoder::getInfo(const std::filesystem::path& path) const {
    return impl_->getInfo(path);
}

std::expected<ImageInfo, DecodeError>
WicDecoder::getInfoFromMemory(std::span<const uint8_t> data) const {
    return impl_->getInfoFromMemory(data);
}

std::expected<DecodedImage, DecodeError> WicDecoder::decode(const std::filesystem::path& path,
                                                            PixelFormat target_format) const {
    return impl_->decode(path, target_format);
}

std::expected<DecodedImage, DecodeError>
WicDecoder::decodeFromMemory(std::span<const uint8_t> data, PixelFormat target_format) const {
    return impl_->decodeFromMemory(data, target_format);
}

std::expected<DecodedImage, DecodeError> WicDecoder::decodeFrame(const std::filesystem::path& path,
                                                                 uint32_t frame_index,
                                                                 PixelFormat target_format) const {
    return impl_->decodeFrame(path, frame_index, target_format);
}

bool WicDecoder::isAvailable() const noexcept {
    return impl_->isAvailable();
}

}  // namespace nive::image
