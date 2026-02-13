/// @file string_utils.hpp
/// @brief String conversion and manipulation utilities
///
/// Provides UTF-8/UTF-16 conversion and other string helpers.

#pragma once

#include <Windows.h>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace nive {

/// @brief String conversion errors
enum class StringError {
    InvalidUtf8,
    InvalidUtf16,
    ConversionFailed,
};

/// @brief Get string representation of string error
[[nodiscard]] constexpr std::string_view to_string(StringError error) noexcept {
    switch (error) {
    case StringError::InvalidUtf8:
        return "Invalid UTF-8 sequence";
    case StringError::InvalidUtf16:
        return "Invalid UTF-16 sequence";
    case StringError::ConversionFailed:
        return "String conversion failed";
    }
    return "Unknown string error";
}

/// @brief Convert UTF-8 string to UTF-16 (wide string)
/// @param utf8 UTF-8 encoded string
/// @return UTF-16 wide string or error
[[nodiscard]] std::expected<std::wstring, StringError> utf8ToWide(std::string_view utf8);

/// @brief Convert UTF-16 (wide string) to UTF-8
/// @param wide UTF-16 encoded wide string
/// @return UTF-8 string or error
[[nodiscard]] std::expected<std::string, StringError> wideToUtf8(std::wstring_view wide);

/// @brief Convert UTF-8 to wide string, or empty string on error
/// @param utf8 UTF-8 encoded string
/// @return UTF-16 wide string (empty on error)
[[nodiscard]] std::wstring utf8ToWideOrEmpty(std::string_view utf8);

/// @brief Convert wide string to UTF-8, or empty string on error
/// @param wide UTF-16 encoded wide string
/// @return UTF-8 string (empty on error)
[[nodiscard]] std::string wideToUtf8OrEmpty(std::wstring_view wide);

/// @brief Convert filesystem path to UTF-8 string
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);

/// @brief Convert UTF-8 string to filesystem path
[[nodiscard]] std::filesystem::path utf8ToPath(std::string_view utf8);

/// @brief Make string lowercase (ASCII only)
[[nodiscard]] std::string toLowercaseAscii(std::string_view str);

/// @brief Make wide string lowercase (ASCII only, for file extensions)
[[nodiscard]] std::wstring toLowercaseAscii(std::wstring_view str);

/// @brief Trim whitespace from both ends of string
[[nodiscard]] std::string_view trim(std::string_view str);

/// @brief Trim whitespace from both ends of wide string
[[nodiscard]] std::wstring_view trim(std::wstring_view str);

/// @brief Check if string starts with prefix (case-sensitive)
[[nodiscard]] bool startsWith(std::string_view str, std::string_view prefix);

/// @brief Check if string ends with suffix (case-sensitive)
[[nodiscard]] bool endsWith(std::string_view str, std::string_view suffix);

/// @brief Check if string starts with prefix (case-insensitive, ASCII)
[[nodiscard]] bool startsWithIcase(std::string_view str, std::string_view prefix);

/// @brief Check if string ends with suffix (case-insensitive, ASCII)
[[nodiscard]] bool endsWithIcase(std::string_view str, std::string_view suffix);

}  // namespace nive
