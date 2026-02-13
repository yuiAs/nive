/// @file file_metadata.cpp
/// @brief File metadata implementation

#include "file_metadata.hpp"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <format>

namespace nive::fs {

namespace {

// Supported image extensions (WIC-compatible + common formats)
constexpr std::array<std::wstring_view, 15> kImageExtensions = {
    L".jpg", L".jpeg", L".png",  L".gif",  L".bmp", L".tiff", L".tif", L".webp",
    L".ico", L".heic", L".heif", L".avif", L".jxr", L".wdp",  L".dds"};

// Supported archive extensions
constexpr std::array<std::wstring_view, 8> kArchiveExtensions = {
    L".zip", L".7z", L".rar", L".lzh", L".lha", L".tar", L".gz", L".cbz"};

[[nodiscard]] std::wstring to_lower(std::wstring_view str) {
    std::wstring result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return result;
}

[[nodiscard]] std::chrono::system_clock::time_point filetime_to_system_clock(const FILETIME& ft) {
    // FILETIME is 100-nanosecond intervals since January 1, 1601
    // system_clock is typically since January 1, 1970
    constexpr int64_t kFileTimeToUnixEpoch = 116444736000000000LL;

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    int64_t ticks = static_cast<int64_t>(uli.QuadPart) - kFileTimeToUnixEpoch;
    auto duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>(ticks);

    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

}  // namespace

std::wstring FileMetadata::sizeString() const {
    constexpr uint64_t kKB = 1024;
    constexpr uint64_t kMB = kKB * 1024;
    constexpr uint64_t kGB = kMB * 1024;
    constexpr uint64_t kTB = kGB * 1024;

    if (size_bytes >= kTB) {
        return std::format(L"{:.2f} TB", static_cast<double>(size_bytes) / kTB);
    } else if (size_bytes >= kGB) {
        return std::format(L"{:.2f} GB", static_cast<double>(size_bytes) / kGB);
    } else if (size_bytes >= kMB) {
        return std::format(L"{:.2f} MB", static_cast<double>(size_bytes) / kMB);
    } else if (size_bytes >= kKB) {
        return std::format(L"{:.2f} KB", static_cast<double>(size_bytes) / kKB);
    } else {
        return std::format(L"{} B", size_bytes);
    }
}

bool isImageExtension(std::wstring_view ext) noexcept {
    if (ext.empty())
        return false;

    std::wstring lower_ext = to_lower(ext);
    for (const auto& image_ext : kImageExtensions) {
        if (lower_ext == image_ext) {
            return true;
        }
    }
    return false;
}

bool isArchiveExtension(std::wstring_view ext) noexcept {
    if (ext.empty())
        return false;

    std::wstring lower_ext = to_lower(ext);
    for (const auto& archive_ext : kArchiveExtensions) {
        if (lower_ext == archive_ext) {
            return true;
        }
    }
    return false;
}

FileType getFileType(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        return FileType::Directory;
    }

    auto ext = path.extension().wstring();
    if (isImageExtension(ext)) {
        return FileType::Image;
    }
    if (isArchiveExtension(ext)) {
        return FileType::Archive;
    }

    return FileType::Other;
}

std::optional<FileMetadata> getFileMetadata(const std::filesystem::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return std::nullopt;
    }

    FileMetadata metadata;
    metadata.path = path;
    metadata.name = path.filename().wstring();
    metadata.extension = path.extension().wstring();
    metadata.type = getFileType(path);

    // Attributes
    metadata.attributes.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    metadata.attributes.is_hidden = (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    metadata.attributes.is_system = (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
    metadata.attributes.is_readonly = (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    metadata.attributes.is_archive = (data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0;
    metadata.attributes.is_compressed = (data.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;
    metadata.attributes.is_encrypted = (data.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) != 0;

    // Size
    ULARGE_INTEGER size;
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;
    metadata.size_bytes = size.QuadPart;

    // Times
    metadata.created_time = filetime_to_system_clock(data.ftCreationTime);
    metadata.modified_time = filetime_to_system_clock(data.ftLastWriteTime);
    metadata.accessed_time = filetime_to_system_clock(data.ftLastAccessTime);

    return metadata;
}

}  // namespace nive::fs
