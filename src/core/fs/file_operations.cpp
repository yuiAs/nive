/// @file file_operations.cpp
/// @brief File operations implementation

#include "file_operations.hpp"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <format>
#include <random>

#include "trash.hpp"

namespace nive::fs {

/// @brief Convert Windows error to FileOperationError
[[nodiscard]] static FileOperationError windows_error_to_file_error(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return FileOperationError::NotFound;

    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return FileOperationError::AccessDenied;

    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return FileOperationError::AlreadyExists;

    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return FileOperationError::DiskFull;

    case ERROR_FILENAME_EXCED_RANGE:
        return FileOperationError::PathTooLong;

    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
        return FileOperationError::InvalidPath;

    default:
        return FileOperationError::IoError;
    }
}

/// @brief Get file size
[[nodiscard]] static uint64_t get_file_size(const std::filesystem::path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

namespace {

/// @brief Invalid filename characters on Windows
constexpr std::array<wchar_t, 9> kInvalidFilenameChars = {L'<',  L'>', L':', L'"', L'/',
                                                          L'\\', L'|', L'?', L'*'};

/// @brief Reserved Windows filenames
constexpr std::array<std::wstring_view, 22> kReservedNames = {
    L"CON",  L"PRN",  L"AUX",  L"NUL",  L"COM1", L"COM2", L"COM3", L"COM4",
    L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
    L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"};

}  // namespace

std::expected<void, FileOperationError> copyFile(const std::filesystem::path& source,
                                                 const std::filesystem::path& dest,
                                                 const CopyOptions& options) {
    if (!std::filesystem::exists(source)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    // Check if destination exists
    if (std::filesystem::exists(dest)) {
        if (options.skip_existing) {
            return {};  // Success - skipped
        }
        if (!options.overwrite_existing) {
            return std::unexpected(FileOperationError::AlreadyExists);
        }
    }

    // Ensure destination directory exists
    auto dest_dir = dest.parent_path();
    if (!dest_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dest_dir, ec);
        if (ec) {
            return std::unexpected(FileOperationError::IoError);
        }
    }

    // Copy file
    BOOL result =
        CopyFileW(source.c_str(), dest.c_str(), options.overwrite_existing ? FALSE : TRUE);

    if (!result) {
        return std::unexpected(windows_error_to_file_error(GetLastError()));
    }

    // Preserve timestamps if requested
    if (options.preserve_timestamps) {
        WIN32_FILE_ATTRIBUTE_DATA source_data;
        if (GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &source_data)) {
            HANDLE dest_handle =
                CreateFileW(dest.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (dest_handle != INVALID_HANDLE_VALUE) {
                SetFileTime(dest_handle, &source_data.ftCreationTime, &source_data.ftLastAccessTime,
                            &source_data.ftLastWriteTime);
                CloseHandle(dest_handle);
            }
        }
    }

    return {};
}

FileOperationResult copyFiles(std::span<const std::filesystem::path> sources,
                              const std::filesystem::path& dest_dir, const CopyOptions& options,
                              ProgressCallback progress) {
    FileOperationResult result;

    // Calculate total size
    uint64_t total_size = 0;
    for (const auto& source : sources) {
        total_size += get_file_size(source);
    }

    uint64_t processed_size = 0;

    for (const auto& source : sources) {
        auto dest = dest_dir / source.filename();

        // Progress callback
        if (progress && !progress(source, processed_size, total_size)) {
            result.error = FileOperationError::Cancelled;
            return result;
        }

        auto copy_result = copyFile(source, dest, options);
        if (copy_result) {
            ++result.files_processed;
            auto file_size = get_file_size(source);
            result.bytes_processed += file_size;
            processed_size += file_size;
        } else {
            result.failed_files.push_back(source);
            processed_size += get_file_size(source);
        }
    }

    if (!result.failed_files.empty()) {
        result.error = result.files_processed > 0 ? FileOperationError::PartialSuccess
                                                  : FileOperationError::IoError;
    }

    return result;
}

std::expected<void, FileOperationError> moveFile(const std::filesystem::path& source,
                                                 const std::filesystem::path& dest,
                                                 const CopyOptions& options) {
    if (!std::filesystem::exists(source)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    // Check if destination exists
    if (std::filesystem::exists(dest)) {
        if (options.skip_existing) {
            return {};
        }
        if (!options.overwrite_existing) {
            return std::unexpected(FileOperationError::AlreadyExists);
        }
    }

    // Ensure destination directory exists
    auto dest_dir = dest.parent_path();
    if (!dest_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dest_dir, ec);
        if (ec) {
            return std::unexpected(FileOperationError::IoError);
        }
    }

    // Try fast move first (same volume)
    DWORD flags = MOVEFILE_COPY_ALLOWED;
    if (options.overwrite_existing) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }

    BOOL result = MoveFileExW(source.c_str(), dest.c_str(), flags);
    if (!result) {
        return std::unexpected(windows_error_to_file_error(GetLastError()));
    }

    return {};
}

FileOperationResult moveFiles(std::span<const std::filesystem::path> sources,
                              const std::filesystem::path& dest_dir, const CopyOptions& options,
                              ProgressCallback progress) {
    FileOperationResult result;

    uint64_t total_size = 0;
    for (const auto& source : sources) {
        total_size += get_file_size(source);
    }

    uint64_t processed_size = 0;

    for (const auto& source : sources) {
        auto dest = dest_dir / source.filename();

        if (progress && !progress(source, processed_size, total_size)) {
            result.error = FileOperationError::Cancelled;
            return result;
        }

        auto move_result = moveFile(source, dest, options);
        if (move_result) {
            ++result.files_processed;
            auto file_size = get_file_size(source);
            result.bytes_processed += file_size;
            processed_size += file_size;
        } else {
            result.failed_files.push_back(source);
            processed_size += get_file_size(source);
        }
    }

    if (!result.failed_files.empty()) {
        result.error = result.files_processed > 0 ? FileOperationError::PartialSuccess
                                                  : FileOperationError::IoError;
    }

    return result;
}

std::expected<void, FileOperationError> deleteFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        // Only delete empty directories
        if (!std::filesystem::is_empty(path, ec)) {
            return std::unexpected(FileOperationError::AccessDenied);
        }
        std::filesystem::remove(path, ec);
    } else {
        std::filesystem::remove(path, ec);
    }

    if (ec) {
        return std::unexpected(FileOperationError::IoError);
    }

    return {};
}

FileOperationResult deleteFiles(std::span<const std::filesystem::path> paths,
                                ProgressCallback progress) {
    FileOperationResult result;

    uint64_t total = paths.size();
    uint64_t current = 0;

    for (const auto& path : paths) {
        if (progress && !progress(path, current, total)) {
            result.error = FileOperationError::Cancelled;
            return result;
        }

        auto delete_result = deleteFile(path);
        if (delete_result) {
            ++result.files_processed;
        } else {
            result.failed_files.push_back(path);
        }

        ++current;
    }

    if (!result.failed_files.empty()) {
        result.error = result.files_processed > 0 ? FileOperationError::PartialSuccess
                                                  : FileOperationError::IoError;
    }

    return result;
}

std::expected<uint64_t, FileOperationError> deleteDirectory(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    std::error_code ec;
    auto count = std::filesystem::remove_all(path, ec);

    if (ec) {
        return std::unexpected(FileOperationError::IoError);
    }

    return count;
}

std::expected<std::filesystem::path, FileOperationError>
renameFile(const std::filesystem::path& path, const std::wstring& new_name) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    if (!isValidFilename(new_name)) {
        return std::unexpected(FileOperationError::InvalidPath);
    }

    auto new_path = path.parent_path() / new_name;

    if (std::filesystem::exists(new_path)) {
        return std::unexpected(FileOperationError::AlreadyExists);
    }

    std::error_code ec;
    std::filesystem::rename(path, new_path, ec);

    if (ec) {
        return std::unexpected(FileOperationError::IoError);
    }

    return new_path;
}

std::expected<void, FileOperationError> createDirectory(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);

    if (ec) {
        return std::unexpected(windows_error_to_file_error(ec.value()));
    }

    return {};
}

bool isValidPath(const std::filesystem::path& path) noexcept {
    auto str = path.wstring();

    // Check length (MAX_PATH is 260, but we allow longer with \\?\ prefix)
    if (str.length() > 32767) {
        return false;
    }

    // Check each component
    for (const auto& part : path) {
        auto name = part.wstring();
        if (name.empty())
            continue;

        // Skip drive letter
        if (name.length() == 2 && name[1] == L':')
            continue;

        if (!isValidFilename(name)) {
            return false;
        }
    }

    return true;
}

bool isValidFilename(std::wstring_view name) noexcept {
    if (name.empty() || name.length() > 255) {
        return false;
    }

    // Check for invalid characters
    for (wchar_t c : name) {
        if (c < 32) {  // Control characters
            return false;
        }
        for (wchar_t invalid : kInvalidFilenameChars) {
            if (c == invalid) {
                return false;
            }
        }
    }

    // Check for trailing spaces or periods
    if (name.back() == L' ' || name.back() == L'.') {
        return false;
    }

    // Check for reserved names
    std::wstring upper_name(name);
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });

    // Remove extension for comparison
    auto dot_pos = upper_name.rfind(L'.');
    if (dot_pos != std::wstring::npos) {
        upper_name = upper_name.substr(0, dot_pos);
    }

    for (const auto& reserved : kReservedNames) {
        if (upper_name == reserved) {
            return false;
        }
    }

    return true;
}

std::filesystem::path generateUniquePath(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return path;
    }

    auto parent = path.parent_path();
    auto stem = path.stem().wstring();
    auto extension = path.extension().wstring();

    for (int i = 1; i < 10000; ++i) {
        auto new_name = std::format(L"{} ({}){}", stem, i, extension);
        auto new_path = parent / new_name;

        if (!std::filesystem::exists(new_path)) {
            return new_path;
        }
    }

    // Fallback: use timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    auto new_name = std::format(L"{}_{}{}", stem, timestamp, extension);
    return parent / new_name;
}

namespace {

/// @brief Generate a temporary file path in the same directory
[[nodiscard]] std::filesystem::path generate_temp_path(const std::filesystem::path& dest) {
    static std::mt19937_64 rng(std::random_device{}());
    auto temp_name = std::format(L".nive_tmp_{:016x}", rng());
    return dest.parent_path() / temp_name;
}

/// @brief Handle a single file conflict according to resolution
/// @return The destination path to use, or empty path to skip
[[nodiscard]] std::filesystem::path resolveConflict(const FileConflictInfo& conflict,
                                                    const ConflictResolution& resolution,
                                                    bool move_replaced_to_trash) {
    switch (resolution.action) {
    case ConflictAction::Skip:
        return {};  // Skip this file

    case ConflictAction::Overwrite:
        // Move existing file to trash if requested
        if (move_replaced_to_trash || resolution.move_replaced_to_trash) {
            (void)trashFile(conflict.dest_path, TrashOptions{.silent = true});
        }
        return conflict.dest_path;

    case ConflictAction::Rename:
        return conflict.dest_path.parent_path() / resolution.custom_name;

    case ConflictAction::AutoNumber:
        return generateUniquePath(conflict.dest_path);

    case ConflictAction::KeepNewer:
        if (conflict.source_time > conflict.dest_time) {
            if (move_replaced_to_trash || resolution.move_replaced_to_trash) {
                (void)trashFile(conflict.dest_path, TrashOptions{.silent = true});
            }
            return conflict.dest_path;
        }
        return {};  // Skip - destination is newer

    case ConflictAction::KeepLarger:
        if (conflict.source_size > conflict.dest_size) {
            if (move_replaced_to_trash || resolution.move_replaced_to_trash) {
                (void)trashFile(conflict.dest_path, TrashOptions{.silent = true});
            }
            return conflict.dest_path;
        }
        return {};  // Skip - destination is larger

    default:
        return {};
    }
}

}  // anonymous namespace

std::expected<void, FileOperationError> safeCopyFile(const std::filesystem::path& source,
                                                     const std::filesystem::path& dest,
                                                     const CopyOptions& options) {
    if (!std::filesystem::exists(source)) {
        return std::unexpected(FileOperationError::NotFound);
    }

    // Check if destination exists
    bool dest_exists = std::filesystem::exists(dest);
    if (dest_exists) {
        if (options.skip_existing) {
            return {};  // Success - skipped
        }
        if (!options.overwrite_existing) {
            return std::unexpected(FileOperationError::AlreadyExists);
        }
    }

    // Ensure destination directory exists
    auto dest_dir = dest.parent_path();
    if (!dest_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dest_dir, ec);
        if (ec) {
            return std::unexpected(FileOperationError::IoError);
        }
    }

    // Generate temporary file path in same directory (for atomic rename)
    auto temp_path = generate_temp_path(dest);

    // Copy to temporary file
    BOOL result = CopyFileW(source.c_str(), temp_path.c_str(), FALSE);
    if (!result) {
        return std::unexpected(windows_error_to_file_error(GetLastError()));
    }

    // Preserve timestamps if requested
    if (options.preserve_timestamps) {
        WIN32_FILE_ATTRIBUTE_DATA source_data;
        if (GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &source_data)) {
            HANDLE temp_handle = CreateFileW(temp_path.c_str(), FILE_WRITE_ATTRIBUTES,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (temp_handle != INVALID_HANDLE_VALUE) {
                SetFileTime(temp_handle, &source_data.ftCreationTime, &source_data.ftLastAccessTime,
                            &source_data.ftLastWriteTime);
                CloseHandle(temp_handle);
            }
        }
    }

    // Atomic rename: temp -> dest
    DWORD move_flags = MOVEFILE_COPY_ALLOWED;
    if (dest_exists) {
        move_flags |= MOVEFILE_REPLACE_EXISTING;
    }

    result = MoveFileExW(temp_path.c_str(), dest.c_str(), move_flags);
    if (!result) {
        // Clean up temp file on failure
        DeleteFileW(temp_path.c_str());
        return std::unexpected(windows_error_to_file_error(GetLastError()));
    }

    return {};
}

FileOperationResult copyFilesWithConflictHandling(std::span<const std::filesystem::path> sources,
                                                  const std::filesystem::path& dest_dir,
                                                  const ExtendedCopyOptions& options,
                                                  ProgressCallback progress) {
    FileOperationResult result;

    // Calculate total size
    uint64_t total_size = 0;
    for (const auto& source : sources) {
        total_size += get_file_size(source);
    }

    uint64_t processed_size = 0;
    std::optional<ConflictResolution> apply_to_all_resolution;

    for (const auto& source : sources) {
        auto dest = dest_dir / source.filename();

        // Progress callback
        if (progress && !progress(source, processed_size, total_size)) {
            result.error = FileOperationError::Cancelled;
            return result;
        }

        // Check for conflict
        if (std::filesystem::exists(dest)) {
            // Build conflict info
            FileConflictInfo conflict;
            conflict.source_path = source;
            conflict.dest_path = dest;

            std::error_code ec;
            conflict.source_size = std::filesystem::file_size(source, ec);
            conflict.dest_size = std::filesystem::file_size(dest, ec);
            conflict.source_time = std::filesystem::last_write_time(source, ec);
            conflict.dest_time = std::filesystem::last_write_time(dest, ec);
            conflict.files_identical = areFilesIdentical(source, dest);

            // Skip identical files if option is set
            if ((options.skip_identical ||
                 (apply_to_all_resolution && apply_to_all_resolution->skip_identical)) &&
                conflict.files_identical) {
                ++result.files_processed;
                processed_size += get_file_size(source);
                continue;
            }

            // Use "apply to all" resolution if set
            ConflictResolution resolution;
            if (apply_to_all_resolution) {
                resolution = *apply_to_all_resolution;
            } else if (options.on_conflict) {
                auto maybe_resolution = options.on_conflict(conflict);
                if (!maybe_resolution) {
                    result.error = FileOperationError::Cancelled;
                    return result;
                }
                resolution = *maybe_resolution;

                if (resolution.apply_to_all) {
                    apply_to_all_resolution = resolution;
                }
            } else {
                // No conflict handler, skip by default
                result.failed_files.push_back(source);
                processed_size += get_file_size(source);
                continue;
            }

            // Resolve the conflict
            auto resolved_dest =
                resolveConflict(conflict, resolution, options.move_replaced_to_trash);
            if (resolved_dest.empty()) {
                // Skip this file
                ++result.files_processed;
                processed_size += get_file_size(source);
                continue;
            }
            dest = resolved_dest;
        }

        // Perform the copy
        CopyOptions copy_opts = options;
        copy_opts.overwrite_existing = true;  // We handled conflicts above

        auto copy_result = safeCopyFile(source, dest, copy_opts);
        if (copy_result) {
            ++result.files_processed;
            auto file_size = get_file_size(source);
            result.bytes_processed += file_size;
            processed_size += file_size;
        } else {
            result.failed_files.push_back(source);
            processed_size += get_file_size(source);
        }
    }

    if (!result.failed_files.empty()) {
        result.error = result.files_processed > 0 ? FileOperationError::PartialSuccess
                                                  : FileOperationError::IoError;
    }

    return result;
}

FileOperationResult moveFilesWithConflictHandling(std::span<const std::filesystem::path> sources,
                                                  const std::filesystem::path& dest_dir,
                                                  const ExtendedCopyOptions& options,
                                                  ProgressCallback progress) {
    FileOperationResult result;

    uint64_t total_size = 0;
    for (const auto& source : sources) {
        total_size += get_file_size(source);
    }

    uint64_t processed_size = 0;
    std::optional<ConflictResolution> apply_to_all_resolution;

    for (const auto& source : sources) {
        auto dest = dest_dir / source.filename();

        if (progress && !progress(source, processed_size, total_size)) {
            result.error = FileOperationError::Cancelled;
            return result;
        }

        // Check for conflict
        if (std::filesystem::exists(dest)) {
            // Build conflict info
            FileConflictInfo conflict;
            conflict.source_path = source;
            conflict.dest_path = dest;

            std::error_code ec;
            conflict.source_size = std::filesystem::file_size(source, ec);
            conflict.dest_size = std::filesystem::file_size(dest, ec);
            conflict.source_time = std::filesystem::last_write_time(source, ec);
            conflict.dest_time = std::filesystem::last_write_time(dest, ec);
            conflict.files_identical = areFilesIdentical(source, dest);

            // Skip identical files if option is set
            if ((options.skip_identical ||
                 (apply_to_all_resolution && apply_to_all_resolution->skip_identical)) &&
                conflict.files_identical) {
                // For move: delete source since dest is identical
                std::filesystem::remove(source, ec);
                ++result.files_processed;
                processed_size += get_file_size(source);
                continue;
            }

            // Use "apply to all" resolution if set
            ConflictResolution resolution;
            if (apply_to_all_resolution) {
                resolution = *apply_to_all_resolution;
            } else if (options.on_conflict) {
                auto maybe_resolution = options.on_conflict(conflict);
                if (!maybe_resolution) {
                    result.error = FileOperationError::Cancelled;
                    return result;
                }
                resolution = *maybe_resolution;

                if (resolution.apply_to_all) {
                    apply_to_all_resolution = resolution;
                }
            } else {
                // No conflict handler, skip by default
                result.failed_files.push_back(source);
                processed_size += get_file_size(source);
                continue;
            }

            // Resolve the conflict
            auto resolved_dest =
                resolveConflict(conflict, resolution, options.move_replaced_to_trash);
            if (resolved_dest.empty()) {
                // Skip this file
                ++result.files_processed;
                processed_size += get_file_size(source);
                continue;
            }
            dest = resolved_dest;
        }

        // Perform the move
        CopyOptions move_opts = options;
        move_opts.overwrite_existing = true;  // We handled conflicts above

        auto move_result = moveFile(source, dest, move_opts);
        if (move_result) {
            ++result.files_processed;
            auto file_size = get_file_size(source);
            result.bytes_processed += file_size;
            processed_size += file_size;
        } else {
            result.failed_files.push_back(source);
            processed_size += get_file_size(source);
        }
    }

    if (!result.failed_files.empty()) {
        result.error = result.files_processed > 0 ? FileOperationError::PartialSuccess
                                                  : FileOperationError::IoError;
    }

    return result;
}

}  // namespace nive::fs
