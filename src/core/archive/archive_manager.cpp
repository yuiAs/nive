/// @file archive_manager.cpp
/// @brief Archive manager implementation

#include "archive_manager.hpp"
#include <Windows.h>

#include <algorithm>
#include <random>

#include "../fs/natural_sort.hpp"
#include "../util/logger.hpp"
#include "../util/string_utils.hpp"

namespace nive::archive {

namespace {

/// @brief Generate random filename for temp file
[[nodiscard]] std::wstring generate_temp_filename(const std::wstring& extension) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist;

    auto random_part = dist(gen);
    return std::format(L"nive_extract_{:08x}{}", random_part, extension);
}

/// @brief Get system temp directory
[[nodiscard]] std::filesystem::path get_system_temp_dir() {
    wchar_t temp_path[MAX_PATH];
    if (GetTempPathW(MAX_PATH, temp_path) > 0) {
        return std::filesystem::path(temp_path) / L"nive";
    }
    return std::filesystem::current_path() / L"temp";
}

}  // namespace

ArchiveManager::ArchiveManager(ArchiveManagerConfig config) : config_(std::move(config)) {
    if (config_.temp_dir.empty()) {
        config_.temp_dir = get_system_temp_dir();
    }
}

ArchiveManager::~ArchiveManager() {
    cleanupTempFiles();
}

bool ArchiveManager::isAvailable() const noexcept {
    return ArchiveReaderFactory::isAvailable();
}

std::optional<std::filesystem::path> ArchiveManager::getDllPath() const noexcept {
    return ArchiveReaderFactory::getDllPath();
}

std::expected<ArchiveInfo, ArchiveError> ArchiveManager::open(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);

    auto reader_result = getReader(path);
    if (!reader_result) {
        LOG_ERROR("Failed to open archive: {} ({})", pathToUtf8(path),
                  to_string(reader_result.error()));
        return std::unexpected(reader_result.error());
    }

    return reader_result.value()->getInfo();
}

bool ArchiveManager::isArchive(const std::filesystem::path& path) const noexcept {
    return is_supported_archive(path);
}

std::expected<std::vector<ArchiveEntry>, ArchiveError>
ArchiveManager::listEntries(const std::filesystem::path& archive_path) {
    std::lock_guard lock(mutex_);

    auto reader_result = getReader(archive_path);
    if (!reader_result) {
        return std::unexpected(reader_result.error());
    }

    return reader_result.value()->listEntries();
}

std::expected<std::vector<ArchiveEntry>, ArchiveError>
ArchiveManager::getImageEntries(const std::filesystem::path& archive_path) {
    auto entries_result = listEntries(archive_path);
    if (!entries_result) {
        return std::unexpected(entries_result.error());
    }

    std::vector<ArchiveEntry> image_entries;
    for (auto& entry : *entries_result) {
        if (entry.is_image()) {
            image_entries.push_back(std::move(entry));
        }
    }

    // Sort by natural order
    std::sort(image_entries.begin(), image_entries.end(),
              [](const ArchiveEntry& a, const ArchiveEntry& b) {
                  return fs::naturalCompare(a.path, b.path) < 0;
              });

    LOG_DEBUG("Found {} image entries in archive", image_entries.size());
    return image_entries;
}

std::expected<std::vector<uint8_t>, ArchiveError>
ArchiveManager::extractToMemory(const VirtualPath& virtual_path) {
    if (!virtual_path.is_in_archive()) {
        return std::unexpected(ArchiveError::InternalError);
    }

    std::lock_guard lock(mutex_);

    auto reader_result = getReader(virtual_path.archive_path());
    if (!reader_result) {
        return std::unexpected(reader_result.error());
    }

    return reader_result.value()->extractToMemory(virtual_path.internal_path());
}

std::expected<std::filesystem::path, ArchiveError>
ArchiveManager::extractToTemp(const VirtualPath& virtual_path) {
    if (!virtual_path.is_in_archive()) {
        return std::unexpected(ArchiveError::InternalError);
    }

    // Generate temp file path
    std::error_code ec;
    std::filesystem::create_directories(config_.temp_dir, ec);
    if (ec) {
        return std::unexpected(ArchiveError::IoError);
    }

    auto temp_path = config_.temp_dir / generate_temp_filename(virtual_path.extension());

    // Extract to temp file
    auto result = extractToFile(virtual_path, temp_path);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Track temp file for cleanup
    {
        std::lock_guard lock(mutex_);
        temp_files_.push_back(temp_path);
    }

    return temp_path;
}

std::expected<void, ArchiveError>
ArchiveManager::extractToFile(const VirtualPath& virtual_path,
                              const std::filesystem::path& dest_path,
                              ExtractProgressCallback progress) {
    if (!virtual_path.is_in_archive()) {
        return std::unexpected(ArchiveError::InternalError);
    }

    std::lock_guard lock(mutex_);

    auto reader_result = getReader(virtual_path.archive_path());
    if (!reader_result) {
        return std::unexpected(reader_result.error());
    }

    return reader_result.value()->extractToFile(virtual_path.internal_path(), dest_path, progress);
}

std::expected<void, ArchiveError>
ArchiveManager::extractAll(const std::filesystem::path& archive_path,
                           const std::filesystem::path& dest_dir,
                           ExtractProgressCallback progress) {
    std::lock_guard lock(mutex_);

    auto reader_result = getReader(archive_path);
    if (!reader_result) {
        return std::unexpected(reader_result.error());
    }

    return reader_result.value()->extractAll(dest_dir, progress);
}

void ArchiveManager::clearCache() {
    std::lock_guard lock(mutex_);
    cache_.clear();
}

void ArchiveManager::cleanupTempFiles() {
    std::lock_guard lock(mutex_);

    LOG_DEBUG("Cleaning up {} temporary files", temp_files_.size());
    for (const auto& temp_path : temp_files_) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
    }
    temp_files_.clear();

    // Try to remove temp directory if empty
    if (!config_.temp_dir.empty()) {
        std::error_code ec;
        std::filesystem::remove(config_.temp_dir, ec);
    }
}

void ArchiveManager::setPasswordCallback(PasswordCallback callback) {
    std::lock_guard lock(mutex_);
    config_.password_callback = std::move(callback);
}

std::expected<IArchiveReader*, ArchiveError>
ArchiveManager::getReader(const std::filesystem::path& archive_path) {
    // Check cache first
    for (auto& cached : cache_) {
        if (cached.path == archive_path && cached.reader && cached.reader->isOpen()) {
            cached.last_access = std::chrono::steady_clock::now();
            return cached.reader.get();
        }
    }

    // Create new reader
    auto reader_result = ArchiveReaderFactory::create();
    if (!reader_result) {
        return std::unexpected(reader_result.error());
    }

    auto reader = std::move(*reader_result);

    // Open archive
    auto open_result = openWithPasswordRetry(reader.get(), archive_path);
    if (!open_result) {
        return std::unexpected(open_result.error());
    }

    // Add to cache
    if (cache_.size() >= config_.max_cached_archives) {
        // Evict oldest entry
        auto oldest = std::min_element(cache_.begin(), cache_.end(),
                                       [](const CachedReader& a, const CachedReader& b) {
                                           return a.last_access < b.last_access;
                                       });
        if (oldest != cache_.end()) {
            LOG_DEBUG("Evicting cached archive reader: {}", pathToUtf8(oldest->path));
            cache_.erase(oldest);
        }
    }

    cache_.push_back({std::move(reader), archive_path, std::chrono::steady_clock::now()});

    return cache_.back().reader.get();
}

std::expected<void, ArchiveError>
ArchiveManager::openWithPasswordRetry(IArchiveReader* reader,
                                      const std::filesystem::path& archive_path) {
    // Try without password first
    auto result = reader->open(archive_path);
    if (result) {
        return {};
    }

    // Check if password is needed
    if (result.error() != ArchiveError::PasswordRequired &&
        result.error() != ArchiveError::WrongPassword) {
        return std::unexpected(result.error());
    }

    // Request password
    if (!config_.password_callback) {
        return std::unexpected(ArchiveError::PasswordRequired);
    }

    // Retry with password (up to 3 attempts)
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto password = config_.password_callback(archive_path);
        if (!password) {
            // User cancelled
            return std::unexpected(ArchiveError::PasswordRequired);
        }

        result = reader->open(archive_path, *password);
        if (result) {
            return {};
        }

        if (result.error() != ArchiveError::WrongPassword) {
            return std::unexpected(result.error());
        }
    }

    return std::unexpected(ArchiveError::WrongPassword);
}

}  // namespace nive::archive
