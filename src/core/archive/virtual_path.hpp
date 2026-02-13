/// @file virtual_path.hpp
/// @brief Virtual path handling for archive contents
///
/// A virtual path represents a location that may be inside an archive.
/// Format: "C:\path\to\archive.zip|internal/path/to/file.jpg"
/// The pipe character '|' separates the archive path from the internal path.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace nive::archive {

/// @brief Virtual path separator
constexpr wchar_t kVirtualPathSeparator = L'|';

/// @brief Virtual path representing a file that may be inside an archive
class VirtualPath {
public:
    /// @brief Construct empty virtual path
    VirtualPath() = default;

    /// @brief Construct from filesystem path (not inside archive)
    explicit VirtualPath(const std::filesystem::path& path);

    /// @brief Construct from archive path and internal path
    VirtualPath(const std::filesystem::path& archive_path, const std::wstring& internal_path);

    /// @brief Parse virtual path string
    /// @param path_string Path string (may contain '|' separator)
    /// @return Parsed virtual path
    [[nodiscard]] static VirtualPath parse(const std::wstring& path_string);

    /// @brief Check if path is inside an archive
    [[nodiscard]] bool is_in_archive() const noexcept { return !internal_path_.empty(); }

    /// @brief Check if path is a regular filesystem path
    [[nodiscard]] bool is_filesystem_path() const noexcept { return !is_in_archive(); }

    /// @brief Check if path is empty
    [[nodiscard]] bool empty() const noexcept {
        return archive_path_.empty() && internal_path_.empty();
    }

    /// @brief Get the archive path (or regular file path if not in archive)
    [[nodiscard]] const std::filesystem::path& archive_path() const noexcept {
        return archive_path_;
    }

    /// @brief Get the internal path within the archive
    /// @return Internal path, or empty if not in archive
    [[nodiscard]] const std::wstring& internal_path() const noexcept { return internal_path_; }

    /// @brief Get the filename (last component of path)
    [[nodiscard]] std::wstring filename() const;

    /// @brief Get the file extension
    [[nodiscard]] std::wstring extension() const;

    /// @brief Get the parent virtual path
    [[nodiscard]] VirtualPath parent_path() const;

    /// @brief Convert to string representation
    [[nodiscard]] std::wstring to_string() const;

    /// @brief Comparison operators
    [[nodiscard]] bool operator==(const VirtualPath& other) const noexcept;
    [[nodiscard]] bool operator!=(const VirtualPath& other) const noexcept {
        return !(*this == other);
    }
    [[nodiscard]] bool operator<(const VirtualPath& other) const noexcept;

private:
    std::filesystem::path archive_path_;  // Archive file path (or regular file path)
    std::wstring internal_path_;          // Path inside archive (empty if not in archive)
};

/// @brief Join virtual path with a child path
[[nodiscard]] VirtualPath operator/(const VirtualPath& parent, const std::wstring& child);

/// @brief Check if a path string contains virtual path separator
[[nodiscard]] inline bool is_virtual_path_string(const std::wstring& path) {
    return path.find(kVirtualPathSeparator) != std::wstring::npos;
}

}  // namespace nive::archive
