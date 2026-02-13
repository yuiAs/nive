/// @file archive_entry.cpp
/// @brief Archive entry implementation

#include "archive_entry.hpp"

#include <algorithm>
#include <array>
#include <cwctype>

namespace nive::archive {

namespace {

// Supported archive extensions
constexpr std::array<std::pair<std::wstring_view, ArchiveFormat>, 10> kArchiveFormats = {
    {{L".zip", ArchiveFormat::Zip},
     {L".7z", ArchiveFormat::SevenZip},
     {L".rar", ArchiveFormat::Rar},
     {L".lzh", ArchiveFormat::Lzh},
     {L".lha", ArchiveFormat::Lzh},
     {L".tar", ArchiveFormat::Tar},
     {L".gz", ArchiveFormat::GZip},
     {L".tgz", ArchiveFormat::GZip},
     {L".cbz", ArchiveFormat::Cbz},
     {L".cbr", ArchiveFormat::Cbr}}
};

// Image extensions for archive content detection
constexpr std::array<std::wstring_view, 10> kImageExtensions = {
    L".jpg", L".jpeg", L".png", L".gif", L".bmp", L".tiff", L".tif", L".webp", L".avif", L".jxr"};

[[nodiscard]] std::wstring to_lower(std::wstring_view str) {
    std::wstring result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return result;
}

}  // namespace

ArchiveFormat detect_format(const std::filesystem::path& path) noexcept {
    auto ext = to_lower(path.extension().wstring());

    for (const auto& [archive_ext, format] : kArchiveFormats) {
        if (ext == archive_ext) {
            return format;
        }
    }

    return ArchiveFormat::Unknown;
}

bool is_supported_archive(const std::filesystem::path& path) noexcept {
    return detect_format(path) != ArchiveFormat::Unknown;
}

std::wstring ArchiveEntry::parent_path() const {
    auto pos = path.find_last_of(L"/\\");
    if (pos == std::wstring::npos) {
        return L"";
    }
    return path.substr(0, pos);
}

std::wstring ArchiveEntry::extension() const {
    auto pos = name.find_last_of(L'.');
    if (pos == std::wstring::npos || pos == 0) {
        return L"";
    }
    return name.substr(pos);
}

bool ArchiveEntry::is_image() const noexcept {
    if (is_directory) {
        return false;
    }

    auto ext = to_lower(extension());
    for (const auto& image_ext : kImageExtensions) {
        if (ext == image_ext) {
            return true;
        }
    }
    return false;
}

const ArchiveEntry* ArchiveInfo::find_entry(std::wstring_view entry_path) const {
    // Normalize path separators
    std::wstring normalized(entry_path);
    std::replace(normalized.begin(), normalized.end(), L'\\', L'/');

    for (const auto& entry : entries) {
        std::wstring entry_normalized = entry.path;
        std::replace(entry_normalized.begin(), entry_normalized.end(), L'\\', L'/');

        if (entry_normalized == normalized) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<const ArchiveEntry*>
ArchiveInfo::get_entries_in_directory(std::wstring_view dir_path) const {
    std::vector<const ArchiveEntry*> result;

    // Normalize directory path
    std::wstring normalized_dir(dir_path);
    std::replace(normalized_dir.begin(), normalized_dir.end(), L'\\', L'/');

    // Remove trailing slash
    while (!normalized_dir.empty() && normalized_dir.back() == L'/') {
        normalized_dir.pop_back();
    }

    for (const auto& entry : entries) {
        std::wstring entry_parent = entry.parent_path();
        std::replace(entry_parent.begin(), entry_parent.end(), L'\\', L'/');

        if (entry_parent == normalized_dir) {
            result.push_back(&entry);
        }
    }

    return result;
}

std::vector<const ArchiveEntry*> ArchiveInfo::get_image_entries() const {
    std::vector<const ArchiveEntry*> result;

    for (const auto& entry : entries) {
        if (entry.is_image()) {
            result.push_back(&entry);
        }
    }

    return result;
}

}  // namespace nive::archive
