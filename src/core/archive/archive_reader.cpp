/// @file archive_reader.cpp
/// @brief Archive reader implementation using bit7z
///
/// This implementation uses bit7z library to interact with 7z.dll.
/// If bit7z is not available, a stub implementation is provided.
///
/// To enable bit7z support:
/// 1. Download bit7z from https://github.com/rikyoz/bit7z
/// 2. Place headers in third_party/include/bit7z/
/// 3. Ensure 7z.dll is available (from 7-Zip installation)
/// 4. Define NIVE_HAS_BIT7Z in CMake

#include "archive_reader.hpp"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <fstream>

#ifdef NIVE_HAS_BIT7Z
    #include <bit7z/bit7z.hpp>
    #include <bit7z/bitarchivereader.hpp>
    #include <bit7z/bitformat.hpp>
#endif

namespace nive::archive {

namespace {

/// @brief Search paths for 7z.dll
std::optional<std::filesystem::path> find_7z_dll() {
    // Search order:
    // 1. Application directory
    // 2. Program Files\7-Zip
    // 3. System PATH

    // Get application directory
    std::array<wchar_t, MAX_PATH> module_path{};
    if (GetModuleFileNameW(nullptr, module_path.data(), MAX_PATH) > 0) {
        std::filesystem::path app_dir = std::filesystem::path(module_path.data()).parent_path();
        auto dll_path = app_dir / L"7z.dll";
        if (std::filesystem::exists(dll_path)) {
            return dll_path;
        }
    }

    // Check Program Files
    std::array<wchar_t, MAX_PATH> program_files{};
    if (GetEnvironmentVariableW(L"ProgramFiles", program_files.data(), MAX_PATH) > 0) {
        auto dll_path = std::filesystem::path(program_files.data()) / L"7-Zip" / L"7z.dll";
        if (std::filesystem::exists(dll_path)) {
            return dll_path;
        }
    }

    // Check Program Files (x86) for 32-bit 7-Zip
    if (GetEnvironmentVariableW(L"ProgramFiles(x86)", program_files.data(), MAX_PATH) > 0) {
        auto dll_path = std::filesystem::path(program_files.data()) / L"7-Zip" / L"7z.dll";
        if (std::filesystem::exists(dll_path)) {
            return dll_path;
        }
    }

    // Try loading from PATH
    HMODULE test_load = LoadLibraryW(L"7z.dll");
    if (test_load) {
        std::array<wchar_t, MAX_PATH> loaded_path{};
        if (GetModuleFileNameW(test_load, loaded_path.data(), MAX_PATH) > 0) {
            FreeLibrary(test_load);
            return std::filesystem::path(loaded_path.data());
        }
        FreeLibrary(test_load);
    }

    return std::nullopt;
}

/// @brief Normalize path separators to forward slash for consistent comparison
std::wstring normalize_path_separators(std::wstring path) {
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return path;
}

#ifdef NIVE_HAS_BIT7Z

/// @brief Archive reader implementation using bit7z
class Bit7zReader : public IArchiveReader {
public:
    Bit7zReader() = default;
    ~Bit7zReader() override { close(); }

    std::expected<void, ArchiveError> open(const std::filesystem::path& path,
                                           const std::wstring& password) override {
        close();

        if (!std::filesystem::exists(path)) {
            return std::unexpected(ArchiveError::NotFound);
        }

        auto dll_path = find_7z_dll();
        if (!dll_path) {
            return std::unexpected(ArchiveError::DllNotFound);
        }

        try {
            lib_ = std::make_unique<bit7z::Bit7zLibrary>(dll_path->wstring());

            if (!password.empty()) {
                reader_ = std::make_unique<bit7z::BitArchiveReader>(
                    *lib_, path.wstring(), bit7z::BitFormat::Auto, password);
            } else {
                reader_ = std::make_unique<bit7z::BitArchiveReader>(*lib_, path.wstring(),
                                                                    bit7z::BitFormat::Auto);
            }

            archive_path_ = path;
            return {};

        } catch (const bit7z::BitException& e) {
            // Map bit7z exceptions to our error types using error_condition
            auto condition = e.code().default_error_condition();
            if (condition == bit7z::BitFailureSource::WrongPassword) {
                return std::unexpected(ArchiveError::WrongPassword);
            }
            if (condition == bit7z::BitFailureSource::DataError ||
                condition == bit7z::BitFailureSource::CRCError) {
                return std::unexpected(ArchiveError::CorruptedArchive);
            }
            return std::unexpected(ArchiveError::InternalError);
        }
    }

    void close() override {
        reader_.reset();
        lib_.reset();
        archive_path_.clear();
    }

    bool isOpen() const noexcept override { return reader_ != nullptr; }

    std::expected<ArchiveInfo, ArchiveError> getInfo() const override {
        if (!reader_) {
            return std::unexpected(ArchiveError::InternalError);
        }

        try {
            ArchiveInfo info;
            info.path = archive_path_;
            info.format = detect_format(archive_path_);
            info.is_encrypted = reader_->isEncrypted();

            for (const auto& item : *reader_) {
                ArchiveEntry entry;
                entry.path = normalize_path_separators(item.path());
                entry.name = item.name();
                entry.is_directory = item.isDir();
                entry.is_encrypted = item.isEncrypted();
                entry.compressed_size = item.packSize();
                entry.uncompressed_size = item.size();
                entry.modified_time = item.lastWriteTime();
                entry.crc32 = item.crc();

                if (entry.is_directory) {
                    ++info.directory_count;
                } else {
                    ++info.file_count;
                    info.total_compressed_size += entry.compressed_size;
                    info.total_uncompressed_size += entry.uncompressed_size;
                }

                info.entries.push_back(std::move(entry));
            }

            return info;

        } catch (const bit7z::BitException&) {
            return std::unexpected(ArchiveError::InternalError);
        }
    }

    std::expected<std::vector<ArchiveEntry>, ArchiveError> listEntries() const override {
        auto info_result = getInfo();
        if (!info_result) {
            return std::unexpected(info_result.error());
        }
        return std::move(info_result->entries);
    }

    std::expected<std::vector<uint8_t>, ArchiveError>
    extractToMemory(const std::wstring& entry_path) const override {
        if (!reader_) {
            return std::unexpected(ArchiveError::InternalError);
        }

        try {
            // Find item index by path (normalize separators for comparison)
            uint32_t found_index = UINT32_MAX;
            for (const auto& item : *reader_) {
                if (normalize_path_separators(item.path()) == entry_path) {
                    found_index = item.index();
                    break;
                }
            }

            if (found_index == UINT32_MAX) {
                return std::unexpected(ArchiveError::NotFound);
            }

            bit7z::buffer_t buffer;
            reader_->extractTo(buffer, found_index);

            // Convert bit7z::buffer_t to std::vector<uint8_t>
            return std::vector<uint8_t>(buffer.begin(), buffer.end());

        } catch (const bit7z::BitException&) {
            return std::unexpected(ArchiveError::ExtractionFailed);
        }
    }

    std::expected<void, ArchiveError>
    extractToFile(const std::wstring& entry_path, const std::filesystem::path& dest_path,
                  ExtractProgressCallback progress) const override {
        if (!reader_) {
            return std::unexpected(ArchiveError::InternalError);
        }

        try {
            // Find item index by path (normalize separators for comparison)
            uint32_t found_index = UINT32_MAX;
            for (const auto& item : *reader_) {
                if (normalize_path_separators(item.path()) == entry_path) {
                    found_index = item.index();
                    break;
                }
            }

            if (found_index == UINT32_MAX) {
                return std::unexpected(ArchiveError::NotFound);
            }

            // Extract to memory first, then write to file
            bit7z::buffer_t buffer;
            reader_->extractTo(buffer, found_index);

            // Ensure destination directory exists
            std::error_code ec;
            std::filesystem::create_directories(dest_path.parent_path(), ec);

            // Write to file
            std::ofstream out_file(dest_path, std::ios::binary);
            if (!out_file) {
                return std::unexpected(ArchiveError::IoError);
            }
            out_file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            return {};

        } catch (const bit7z::BitException&) {
            return std::unexpected(ArchiveError::ExtractionFailed);
        }
    }

    std::expected<void, ArchiveError> extractAll(const std::filesystem::path& dest_dir,
                                                 ExtractProgressCallback progress) const override {
        if (!reader_) {
            return std::unexpected(ArchiveError::InternalError);
        }

        try {
            std::error_code ec;
            std::filesystem::create_directories(dest_dir, ec);

            if (progress) {
                reader_->setProgressCallback(
                    [&progress](uint64_t current) -> bool { return progress(current, current); });
            }

            reader_->extractTo(dest_dir.wstring());
            return {};

        } catch (const bit7z::BitException&) {
            return std::unexpected(ArchiveError::ExtractionFailed);
        }
    }

    std::expected<void, ArchiveError> test() const override {
        if (!reader_) {
            return std::unexpected(ArchiveError::InternalError);
        }

        try {
            reader_->test();
            return {};
        } catch (const bit7z::BitException&) {
            return std::unexpected(ArchiveError::CorruptedArchive);
        }
    }

private:
    std::unique_ptr<bit7z::Bit7zLibrary> lib_;
    std::unique_ptr<bit7z::BitArchiveReader> reader_;
    std::filesystem::path archive_path_;
};

#else  // !NIVE_HAS_BIT7Z

/// @brief Stub archive reader when bit7z is not available
class StubReader : public IArchiveReader {
public:
    std::expected<void, ArchiveError> open(const std::filesystem::path&,
                                           const std::wstring&) override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    void close() override {}

    bool isOpen() const noexcept override { return false; }

    std::expected<ArchiveInfo, ArchiveError> getInfo() const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    std::expected<std::vector<ArchiveEntry>, ArchiveError> listEntries() const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    std::expected<std::vector<uint8_t>, ArchiveError>
    extractToMemory(const std::wstring&) const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    std::expected<void, ArchiveError> extractToFile(const std::wstring&,
                                                    const std::filesystem::path&,
                                                    ExtractProgressCallback) const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    std::expected<void, ArchiveError> extractAll(const std::filesystem::path&,
                                                 ExtractProgressCallback) const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }

    std::expected<void, ArchiveError> test() const override {
        return std::unexpected(ArchiveError::DllNotFound);
    }
};

#endif  // NIVE_HAS_BIT7Z

}  // namespace

// Factory implementation

std::expected<std::unique_ptr<IArchiveReader>, ArchiveError> ArchiveReaderFactory::create() {
#ifdef NIVE_HAS_BIT7Z
    auto dll_path = find_7z_dll();
    if (!dll_path) {
        return std::unexpected(ArchiveError::DllNotFound);
    }
    return std::make_unique<Bit7zReader>();
#else
    // bit7z not available - check if 7z.dll exists anyway
    auto dll_path = find_7z_dll();
    if (!dll_path) {
        return std::unexpected(ArchiveError::DllNotFound);
    }
    // DLL found but bit7z not compiled in
    return std::make_unique<StubReader>();
#endif
}

bool ArchiveReaderFactory::isAvailable() noexcept {
#ifdef NIVE_HAS_BIT7Z
    return find_7z_dll().has_value();
#else
    return false;
#endif
}

std::optional<std::filesystem::path> ArchiveReaderFactory::getDllPath() noexcept {
    return find_7z_dll();
}

}  // namespace nive::archive
