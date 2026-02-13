/// @file archive_reader.hpp
/// @brief Archive reader interface

#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "archive_entry.hpp"
#include "archive_error.hpp"

namespace nive::archive {

/// @brief Progress callback for extraction
/// @param current Current bytes extracted
/// @param total Total bytes to extract
/// @return false to cancel
using ExtractProgressCallback = std::function<bool(uint64_t current, uint64_t total)>;

/// @brief Archive reader interface
///
/// Abstract interface for reading archive contents.
/// Implementations include Bit7zReader (using 7z.dll).
class IArchiveReader {
public:
    virtual ~IArchiveReader() = default;

    /// @brief Open an archive file
    /// @param path Archive file path
    /// @param password Optional password for encrypted archives
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, ArchiveError>
    open(const std::filesystem::path& path, const std::wstring& password = L"") = 0;

    /// @brief Close the archive
    virtual void close() = 0;

    /// @brief Check if archive is open
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    /// @brief Get archive information
    [[nodiscard]] virtual std::expected<ArchiveInfo, ArchiveError> getInfo() const = 0;

    /// @brief List all entries in the archive
    [[nodiscard]] virtual std::expected<std::vector<ArchiveEntry>, ArchiveError>
    listEntries() const = 0;

    /// @brief Extract a single entry to memory
    /// @param entry_path Path of entry inside archive
    /// @return File contents or error
    [[nodiscard]] virtual std::expected<std::vector<uint8_t>, ArchiveError>
    extractToMemory(const std::wstring& entry_path) const = 0;

    /// @brief Extract a single entry to file
    /// @param entry_path Path of entry inside archive
    /// @param dest_path Destination file path
    /// @param progress Progress callback (optional)
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, ArchiveError>
    extractToFile(const std::wstring& entry_path, const std::filesystem::path& dest_path,
                  ExtractProgressCallback progress = nullptr) const = 0;

    /// @brief Extract all entries to directory
    /// @param dest_dir Destination directory
    /// @param progress Progress callback (optional)
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, ArchiveError>
    extractAll(const std::filesystem::path& dest_dir,
               ExtractProgressCallback progress = nullptr) const = 0;

    /// @brief Test archive integrity
    /// @return Success or error describing the problem
    [[nodiscard]] virtual std::expected<void, ArchiveError> test() const = 0;
};

/// @brief Factory for creating archive readers
class ArchiveReaderFactory {
public:
    /// @brief Create default archive reader
    /// @return Archive reader or error (e.g., DllNotFound)
    [[nodiscard]] static std::expected<std::unique_ptr<IArchiveReader>, ArchiveError> create();

    /// @brief Check if archive support is available
    /// @return true if 7z.dll is loaded and functional
    [[nodiscard]] static bool isAvailable() noexcept;

    /// @brief Get path to 7z.dll if found
    [[nodiscard]] static std::optional<std::filesystem::path> getDllPath() noexcept;
};

}  // namespace nive::archive
