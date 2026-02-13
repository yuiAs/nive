/// @file file_metadata.hpp
/// @brief File metadata structures

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "../archive/virtual_path.hpp"

namespace nive::fs {

/// @brief File type enumeration
enum class FileType {
    Unknown,
    Image,
    Archive,
    Directory,
    Other
};

/// @brief Convert file type to string
[[nodiscard]] constexpr std::string_view to_string(FileType type) noexcept {
    switch (type) {
    case FileType::Image:
        return "Image";
    case FileType::Archive:
        return "Archive";
    case FileType::Directory:
        return "Directory";
    case FileType::Other:
        return "Other";
    default:
        return "Unknown";
    }
}

/// @brief File attributes
struct FileAttributes {
    bool is_directory = false;
    bool is_hidden = false;
    bool is_system = false;
    bool is_readonly = false;
    bool is_archive = false;
    bool is_compressed = false;
    bool is_encrypted = false;
};

/// @brief File metadata
struct FileMetadata {
    std::filesystem::path path;
    std::wstring name;
    std::wstring extension;

    FileType type = FileType::Unknown;
    FileAttributes attributes;

    uint64_t size_bytes = 0;
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point modified_time;
    std::chrono::system_clock::time_point accessed_time;

    // Virtual path for files inside archives
    std::optional<archive::VirtualPath> virtual_path;

    /// @brief Check if file is an image
    [[nodiscard]] bool is_image() const noexcept { return type == FileType::Image; }

    /// @brief Check if file is an archive
    [[nodiscard]] bool is_archive() const noexcept { return type == FileType::Archive; }

    /// @brief Check if this is a directory
    [[nodiscard]] bool is_directory() const noexcept {
        return type == FileType::Directory || attributes.is_directory;
    }

    /// @brief Check if file is inside an archive
    [[nodiscard]] bool is_in_archive() const noexcept {
        return virtual_path.has_value() && virtual_path->is_in_archive();
    }

    /// @brief Get unique identifier for this file (used for thumbnail keying)
    /// Returns virtual path string for archive contents, filesystem path otherwise
    [[nodiscard]] std::wstring sourceIdentifier() const {
        if (is_in_archive()) {
            return virtual_path->to_string();
        }
        return path.wstring();
    }

    /// @brief Get human-readable size string
    [[nodiscard]] std::wstring sizeString() const;
};

/// @brief Supported image extensions
[[nodiscard]] bool isImageExtension(std::wstring_view ext) noexcept;

/// @brief Supported archive extensions
[[nodiscard]] bool isArchiveExtension(std::wstring_view ext) noexcept;

/// @brief Determine file type from extension
[[nodiscard]] FileType getFileType(const std::filesystem::path& path) noexcept;

/// @brief Get file metadata
/// @param path File path
/// @return File metadata or nullopt if file doesn't exist
[[nodiscard]] std::optional<FileMetadata> getFileMetadata(const std::filesystem::path& path);

}  // namespace nive::fs
