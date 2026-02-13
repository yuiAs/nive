/// @file directory.cpp
/// @brief Directory scanning implementation

#include "directory.hpp"
#include <Windows.h>

#include <ShlObj.h>

#include <algorithm>
#include <cwctype>
#include <future>
#include <thread>

#include "natural_sort.hpp"

namespace nive::fs {

namespace {

/// @brief Check if file passes filter
[[nodiscard]] bool passes_filter(const FileMetadata& metadata, const DirectoryFilter& filter) {
    // Hidden files
    if (metadata.attributes.is_hidden && !filter.include_hidden) {
        return false;
    }

    // System files
    if (metadata.attributes.is_system && !filter.include_system) {
        return false;
    }

    // Directory/file only filters
    if (filter.directories_only && !metadata.is_directory()) {
        return false;
    }
    if (filter.files_only && metadata.is_directory()) {
        return false;
    }

    // Type filters
    if (filter.images_only && !metadata.is_image()) {
        return false;
    }
    if (filter.archives_only && !metadata.is_archive()) {
        return false;
    }

    // Extension filter
    if (!filter.extensions.empty() && !metadata.is_directory()) {
        bool found = false;
        std::wstring ext = metadata.extension;
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

        for (const auto& allowed : filter.extensions) {
            std::wstring lower_allowed = allowed;
            std::transform(lower_allowed.begin(), lower_allowed.end(), lower_allowed.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
            if (ext == lower_allowed) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

/// @brief Sort entries according to sort order
void sort_entries(std::vector<FileMetadata>& entries, SortOrder order) {
    auto compare = [order](const FileMetadata& a, const FileMetadata& b) -> bool {
        // Directories always come first (except for type sort)
        if (order != SortOrder::Type && order != SortOrder::TypeDesc) {
            if (a.is_directory() != b.is_directory()) {
                return a.is_directory();
            }
        }

        switch (order) {
        case SortOrder::Natural:
            return naturalCompare(a.name, b.name) < 0;

        case SortOrder::NaturalDesc:
            return naturalCompare(a.name, b.name) > 0;

        case SortOrder::Name:
            return a.name < b.name;

        case SortOrder::NameDesc:
            return a.name > b.name;

        case SortOrder::Size:
            return a.size_bytes < b.size_bytes;

        case SortOrder::SizeDesc:
            return a.size_bytes > b.size_bytes;

        case SortOrder::Modified:
            return a.modified_time < b.modified_time;

        case SortOrder::ModifiedDesc:
            return a.modified_time > b.modified_time;

        case SortOrder::Type: {
            if (a.extension != b.extension) {
                return a.extension < b.extension;
            }
            return naturalCompare(a.name, b.name) < 0;
        }

        case SortOrder::TypeDesc: {
            if (a.extension != b.extension) {
                return a.extension > b.extension;
            }
            return naturalCompare(a.name, b.name) > 0;
        }

        default:
            return naturalCompare(a.name, b.name) < 0;
        }
    };

    std::sort(entries.begin(), entries.end(), compare);
}

}  // namespace

std::expected<DirectoryListing, DirectoryError> scanDirectory(const std::filesystem::path& path,
                                                              const DirectoryFilter& filter,
                                                              SortOrder sort_order) {
    // Check if path exists
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return std::unexpected(DirectoryError::NotFound);
    }

    if (!std::filesystem::is_directory(path, ec)) {
        return std::unexpected(DirectoryError::NotADirectory);
    }

    DirectoryListing listing;
    listing.path = path;

    // Use Windows API for better performance and attribute access
    WIN32_FIND_DATAW find_data;
    std::wstring search_path = path.wstring() + L"\\*";

    HANDLE find_handle = FindFirstFileW(search_path.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            return std::unexpected(DirectoryError::AccessDenied);
        }
        return std::unexpected(DirectoryError::IoError);
    }

    do {
        // Skip . and ..
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        std::filesystem::path entry_path = path / find_data.cFileName;
        auto metadata = getFileMetadata(entry_path);

        if (!metadata) {
            continue;
        }

        // Apply filter
        if (!passes_filter(*metadata, filter)) {
            continue;
        }

        // Update statistics
        if (metadata->is_directory()) {
            ++listing.total_directories;
        } else {
            ++listing.total_files;
            listing.total_size_bytes += metadata->size_bytes;
        }

        listing.entries.push_back(std::move(*metadata));

    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);

    // Sort entries
    sort_entries(listing.entries, sort_order);

    return listing;
}

void scanDirectoryAsync(
    const std::filesystem::path& path, const DirectoryFilter& filter, SortOrder sort_order,
    std::function<void(std::expected<DirectoryListing, DirectoryError>)> callback,
    std::function<void(size_t, size_t)> progress_callback) {
    // Launch async task
    std::thread([path, filter, sort_order, callback = std::move(callback),
                 progress_callback = std::move(progress_callback)]() {
        auto result = scanDirectory(path, filter, sort_order);

        // Progress callback (if provided) - report completion
        if (progress_callback && result) {
            progress_callback(result->entries.size(), result->entries.size());
        }

        callback(std::move(result));
    }).detach();
}

std::expected<std::vector<std::filesystem::path>, DirectoryError>
getSubdirectories(const std::filesystem::path& path, bool include_hidden) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return std::unexpected(DirectoryError::NotFound);
    }

    if (!std::filesystem::is_directory(path, ec)) {
        return std::unexpected(DirectoryError::NotADirectory);
    }

    std::vector<std::filesystem::path> subdirs;

    WIN32_FIND_DATAW find_data;
    std::wstring search_path = path.wstring() + L"\\*";

    HANDLE find_handle = FindFirstFileW(search_path.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(DirectoryError::IoError);
    }

    do {
        // Skip . and ..
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        // Only directories
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }

        // Skip hidden if not requested
        if (!include_hidden && (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
            continue;
        }

        // Skip system directories
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
            continue;
        }

        subdirs.push_back(path / find_data.cFileName);

    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);

    // Sort naturally
    std::sort(subdirs.begin(), subdirs.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  return naturalCompare(a.filename().wstring(), b.filename().wstring()) < 0;
              });

    return subdirs;
}

std::optional<std::filesystem::path> getParentDirectory(const std::filesystem::path& path) {
    if (!path.has_parent_path() || path == path.root_path()) {
        return std::nullopt;
    }
    return path.parent_path();
}

bool isDirectoryEmpty(const std::filesystem::path& path) {
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(path, ec);
    if (ec) {
        return true;  // Treat errors as empty
    }
    return it == std::filesystem::directory_iterator{};
}

std::expected<size_t, DirectoryError> countFiles(const std::filesystem::path& path,
                                                 const DirectoryFilter& filter) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return std::unexpected(DirectoryError::NotFound);
    }

    size_t count = 0;

    WIN32_FIND_DATAW find_data;
    std::wstring search_path = path.wstring() + L"\\*";

    HANDLE find_handle = FindFirstFileW(search_path.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(DirectoryError::IoError);
    }

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        std::filesystem::path entry_path = path / find_data.cFileName;
        auto metadata = getFileMetadata(entry_path);

        if (metadata && passes_filter(*metadata, filter)) {
            ++count;
        }

    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);

    return count;
}

std::vector<std::filesystem::path> getDrives() {
    std::vector<std::filesystem::path> drives;

    DWORD drive_mask = GetLogicalDrives();
    if (drive_mask == 0) {
        return drives;
    }

    wchar_t drive_letter = L'A';
    for (int i = 0; i < 26; ++i) {
        if (drive_mask & (1 << i)) {
            std::wstring drive_path = std::wstring(1, drive_letter) + L":\\";
            drives.emplace_back(drive_path);
        }
        ++drive_letter;
    }

    return drives;
}

std::optional<std::filesystem::path> getSpecialFolder(int folder_id) {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, folder_id, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return std::filesystem::path(path);
    }
    return std::nullopt;
}

}  // namespace nive::fs
