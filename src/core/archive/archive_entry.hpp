/// @file archive_entry.hpp
/// @brief Archive entry structures

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nive::archive {

/// @brief Supported archive formats
enum class ArchiveFormat {
    Unknown,
    Zip,
    SevenZip,
    Rar,
    Lzh,
    Tar,
    GZip,
    Cbz,  // Comic book archive (ZIP-based)
    Cbr   // Comic book archive (RAR-based)
};

/// @brief Convert format to string
[[nodiscard]] constexpr std::string_view to_string(ArchiveFormat format) noexcept {
    switch (format) {
    case ArchiveFormat::Zip:
        return "ZIP";
    case ArchiveFormat::SevenZip:
        return "7z";
    case ArchiveFormat::Rar:
        return "RAR";
    case ArchiveFormat::Lzh:
        return "LZH";
    case ArchiveFormat::Tar:
        return "TAR";
    case ArchiveFormat::GZip:
        return "GZip";
    case ArchiveFormat::Cbz:
        return "CBZ";
    case ArchiveFormat::Cbr:
        return "CBR";
    default:
        return "Unknown";
    }
}

/// @brief Detect archive format from extension
[[nodiscard]] ArchiveFormat detect_format(const std::filesystem::path& path) noexcept;

/// @brief Check if extension is a supported archive format
[[nodiscard]] bool is_supported_archive(const std::filesystem::path& path) noexcept;

/// @brief Archive entry (file or directory inside archive)
struct ArchiveEntry {
    std::wstring path;  // Path inside archive
    std::wstring name;  // Entry name (filename)
    bool is_directory = false;
    bool is_encrypted = false;

    uint64_t compressed_size = 0;
    uint64_t uncompressed_size = 0;
    std::chrono::system_clock::time_point modified_time;

    uint32_t crc32 = 0;
    uint32_t attributes = 0;

    /// @brief Get parent path inside archive
    [[nodiscard]] std::wstring parent_path() const;

    /// @brief Get file extension
    [[nodiscard]] std::wstring extension() const;

    /// @brief Check if this is an image file
    [[nodiscard]] bool is_image() const noexcept;

    /// @brief Get compression ratio (0.0 - 1.0)
    [[nodiscard]] double compression_ratio() const noexcept {
        if (uncompressed_size == 0)
            return 0.0;
        return 1.0 - (static_cast<double>(compressed_size) / uncompressed_size);
    }
};

/// @brief Archive information
struct ArchiveInfo {
    std::filesystem::path path;  // Path to archive file
    ArchiveFormat format = ArchiveFormat::Unknown;
    bool is_encrypted = false;
    bool is_solid = false;  // Solid archive (7z)
    bool is_multi_volume = false;

    uint64_t total_compressed_size = 0;
    uint64_t total_uncompressed_size = 0;
    uint64_t file_count = 0;
    uint64_t directory_count = 0;

    std::vector<ArchiveEntry> entries;

    /// @brief Get total entry count
    [[nodiscard]] uint64_t total_entries() const noexcept { return file_count + directory_count; }

    /// @brief Get overall compression ratio
    [[nodiscard]] double compression_ratio() const noexcept {
        if (total_uncompressed_size == 0)
            return 0.0;
        return 1.0 - (static_cast<double>(total_compressed_size) / total_uncompressed_size);
    }

    /// @brief Find entry by path
    [[nodiscard]] const ArchiveEntry* find_entry(std::wstring_view entry_path) const;

    /// @brief Get entries in a directory
    [[nodiscard]] std::vector<const ArchiveEntry*>
    get_entries_in_directory(std::wstring_view dir_path) const;

    /// @brief Get all image entries
    [[nodiscard]] std::vector<const ArchiveEntry*> get_image_entries() const;
};

}  // namespace nive::archive
