/// @file archive_error.hpp
/// @brief Archive operation error types

#pragma once

#include <string>
#include <string_view>

namespace nive::archive {

/// @brief Archive error types
enum class ArchiveError {
    NotFound,
    AccessDenied,
    UnsupportedFormat,
    CorruptedArchive,
    PasswordRequired,
    WrongPassword,
    ExtractionFailed,
    IoError,
    OutOfMemory,
    DllNotFound,    // 7z.dll not found
    DllLoadFailed,  // Failed to load 7z.dll
    InternalError
};

/// @brief Convert error to string
[[nodiscard]] constexpr std::string_view to_string(ArchiveError error) noexcept {
    switch (error) {
    case ArchiveError::NotFound:
        return "Archive not found";
    case ArchiveError::AccessDenied:
        return "Access denied";
    case ArchiveError::UnsupportedFormat:
        return "Unsupported archive format";
    case ArchiveError::CorruptedArchive:
        return "Corrupted archive";
    case ArchiveError::PasswordRequired:
        return "Password required";
    case ArchiveError::WrongPassword:
        return "Wrong password";
    case ArchiveError::ExtractionFailed:
        return "Extraction failed";
    case ArchiveError::IoError:
        return "I/O error";
    case ArchiveError::OutOfMemory:
        return "Out of memory";
    case ArchiveError::DllNotFound:
        return "7z.dll not found";
    case ArchiveError::DllLoadFailed:
        return "Failed to load 7z.dll";
    case ArchiveError::InternalError:
        return "Internal error";
    default:
        return "Unknown error";
    }
}

/// @brief Get user-friendly error message
[[nodiscard]] inline std::wstring get_error_message(ArchiveError error) {
    switch (error) {
    case ArchiveError::DllNotFound:
        return L"7z.dll was not found. Please install 7-Zip from https://www.7-zip.org/ "
               L"and ensure 7z.dll is in the application directory or system PATH.";

    case ArchiveError::DllLoadFailed:
        return L"Failed to load 7z.dll. The file may be corrupted or incompatible. "
               L"Please reinstall 7-Zip from https://www.7-zip.org/";

    case ArchiveError::PasswordRequired:
        return L"This archive is password protected. Please enter the password.";

    case ArchiveError::WrongPassword:
        return L"The password is incorrect. Please try again.";

    case ArchiveError::CorruptedArchive:
        return L"The archive appears to be corrupted and cannot be opened.";

    case ArchiveError::UnsupportedFormat:
        return L"This archive format is not supported.";

    default:
        return std::wstring(to_string(error).begin(), to_string(error).end());
    }
}

}  // namespace nive::archive
