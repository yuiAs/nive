/// @file file_conflict.cpp
/// @brief File conflict detection and validation implementation

#include "file_conflict.hpp"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <fstream>

namespace nive::fs {

namespace {

/// @brief Size of blocks to compare for identity check
constexpr size_t kHashBlockSize = 4096;

/// @brief Simple hash function for quick file comparison
[[nodiscard]] uint64_t quickHash(const std::vector<char>& data) {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : data) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}

/// @brief Read a block of data from a file at specified position
[[nodiscard]] std::vector<char> readBlock(const std::filesystem::path& path, uint64_t offset,
                                          size_t size) {
    std::vector<char> buffer(size);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    file.seekg(static_cast<std::streamoff>(offset));
    file.read(buffer.data(), static_cast<std::streamsize>(size));

    auto bytes_read = file.gcount();
    buffer.resize(static_cast<size_t>(bytes_read));

    return buffer;
}

}  // namespace

OperationValidation validateOperation(const std::filesystem::path& source,
                                      const std::filesystem::path& dest, bool is_move) {
    OperationValidation result;

    // Check source exists
    std::error_code ec;
    if (!std::filesystem::exists(source, ec)) {
        result.valid = false;
        result.source_exists = false;
        result.error_message = L"Source file does not exist";
        return result;
    }

    // Get destination directory
    auto dest_dir = dest.parent_path();
    if (dest_dir.empty()) {
        dest_dir = L".";
    }

    // Check destination directory is writable
    if (std::filesystem::exists(dest_dir, ec)) {
        // Try to check write access
        auto test_path = dest_dir / L".nive_write_test_tmp";
        HANDLE h = CreateFileW(test_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

        if (h == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_ACCESS_DENIED) {
                result.valid = false;
                result.dest_writable = false;
                result.error_message = L"Destination directory is not writable";
                return result;
            }
        } else {
            CloseHandle(h);
        }
    }

    // Check for circular copy (dest is subdirectory of source)
    if (std::filesystem::is_directory(source, ec)) {
        if (isSubdirectory(source, dest)) {
            result.valid = false;
            result.no_circular_copy = false;
            result.error_message = L"Cannot copy a folder into itself";
            return result;
        }
    }

    // Check available disk space (only for copy, move on same drive doesn't need space)
    auto source_size =
        std::filesystem::is_regular_file(source, ec) ? std::filesystem::file_size(source, ec) : 0;

    if (!is_move || (std::towupper(source.wstring()[0]) != std::towupper(dest.wstring()[0]))) {
        auto available = getAvailableSpace(dest);
        if (available > 0 && source_size > available) {
            result.valid = false;
            result.sufficient_space = false;
            result.error_message = L"Insufficient disk space";
            return result;
        }
    }

    return result;
}

bool areFilesIdentical(const std::filesystem::path& path1, const std::filesystem::path& path2) {
    std::error_code ec;

    // Check if both exist
    if (!std::filesystem::exists(path1, ec) || !std::filesystem::exists(path2, ec)) {
        return false;
    }

    // Compare sizes first (fast check)
    auto size1 = std::filesystem::file_size(path1, ec);
    if (ec)
        return false;

    auto size2 = std::filesystem::file_size(path2, ec);
    if (ec)
        return false;

    if (size1 != size2) {
        return false;
    }

    // Empty files are identical
    if (size1 == 0) {
        return true;
    }

    // Compare first block
    auto block1_start = readBlock(path1, 0, kHashBlockSize);
    auto block2_start = readBlock(path2, 0, kHashBlockSize);

    if (block1_start != block2_start) {
        return false;
    }

    // For small files, we're done
    if (size1 <= kHashBlockSize) {
        return true;
    }

    // Compare last block
    uint64_t last_offset = size1 > kHashBlockSize ? size1 - kHashBlockSize : 0;
    auto block1_end = readBlock(path1, last_offset, kHashBlockSize);
    auto block2_end = readBlock(path2, last_offset, kHashBlockSize);

    if (block1_end != block2_end) {
        return false;
    }

    // For large files, also check a middle block
    if (size1 > kHashBlockSize * 3) {
        uint64_t mid_offset = size1 / 2;
        auto block1_mid = readBlock(path1, mid_offset, kHashBlockSize);
        auto block2_mid = readBlock(path2, mid_offset, kHashBlockSize);

        if (block1_mid != block2_mid) {
            return false;
        }
    }

    return true;
}

std::vector<FileConflictInfo> detectConflicts(const std::vector<std::filesystem::path>& sources,
                                              const std::filesystem::path& dest_dir) {
    std::vector<FileConflictInfo> conflicts;
    std::error_code ec;

    for (const auto& source : sources) {
        auto dest = dest_dir / source.filename();

        if (std::filesystem::exists(dest, ec)) {
            FileConflictInfo info;
            info.source_path = source;
            info.dest_path = dest;
            info.source_size = std::filesystem::file_size(source, ec);
            info.dest_size = std::filesystem::file_size(dest, ec);
            info.source_time = std::filesystem::last_write_time(source, ec);
            info.dest_time = std::filesystem::last_write_time(dest, ec);
            info.files_identical = areFilesIdentical(source, dest);

            conflicts.push_back(std::move(info));
        }
    }

    return conflicts;
}

uint64_t getAvailableSpace(const std::filesystem::path& path) {
    std::error_code ec;
    auto space_info = std::filesystem::space(path, ec);

    if (ec) {
        return 0;
    }

    return space_info.available;
}

uint64_t calculateTotalSize(const std::vector<std::filesystem::path>& paths) {
    uint64_t total = 0;
    std::error_code ec;

    for (const auto& path : paths) {
        if (std::filesystem::is_regular_file(path, ec)) {
            total += std::filesystem::file_size(path, ec);
        } else if (std::filesystem::is_directory(path, ec)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
                if (entry.is_regular_file(ec)) {
                    total += entry.file_size(ec);
                }
            }
        }
    }

    return total;
}

bool isSubdirectory(const std::filesystem::path& source, const std::filesystem::path& dest) {
    std::error_code ec;

    // Get canonical paths for comparison
    auto source_canonical = std::filesystem::weakly_canonical(source, ec);
    if (ec)
        return false;

    auto dest_canonical = std::filesystem::weakly_canonical(dest, ec);
    if (ec)
        return false;

    // Check if dest starts with source
    auto source_str = source_canonical.wstring();
    auto dest_str = dest_canonical.wstring();

    // Normalize to lowercase for Windows
    std::transform(source_str.begin(), source_str.end(), source_str.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    std::transform(dest_str.begin(), dest_str.end(), dest_str.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

    // Ensure source ends with separator for proper prefix matching
    if (!source_str.empty() && source_str.back() != L'\\' && source_str.back() != L'/') {
        source_str += L'\\';
    }

    return dest_str.starts_with(source_str);
}

}  // namespace nive::fs
