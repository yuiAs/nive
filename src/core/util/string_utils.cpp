/// @file string_utils.cpp
/// @brief String conversion and manipulation utilities implementation

#include "string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <ranges>

namespace nive {

std::expected<std::wstring, StringError> utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return std::wstring{};
    }

    // Calculate required buffer size
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                   static_cast<int>(utf8.size()), nullptr, 0);

    if (size <= 0) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            return std::unexpected(StringError::InvalidUtf8);
        }
        return std::unexpected(StringError::ConversionFailed);
    }

    // Perform conversion
    std::wstring result(static_cast<size_t>(size), L'\0');
    int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                        static_cast<int>(utf8.size()), result.data(), size);

    if (converted <= 0) {
        return std::unexpected(StringError::ConversionFailed);
    }

    return result;
}

std::expected<std::string, StringError> wideToUtf8(std::wstring_view wide) {
    if (wide.empty()) {
        return std::string{};
    }

    // Calculate required buffer size
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                                   static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);

    if (size <= 0) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            return std::unexpected(StringError::InvalidUtf16);
        }
        return std::unexpected(StringError::ConversionFailed);
    }

    // Perform conversion
    std::string result(static_cast<size_t>(size), '\0');
    int converted =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                            static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);

    if (converted <= 0) {
        return std::unexpected(StringError::ConversionFailed);
    }

    return result;
}

std::wstring utf8ToWide_or_empty(std::string_view utf8) {
    auto result = utf8ToWide(utf8);
    return result ? *result : std::wstring{};
}

std::string wideToUtf8_or_empty(std::wstring_view wide) {
    auto result = wideToUtf8(wide);
    return result ? *result : std::string{};
}

std::string pathToUtf8(const std::filesystem::path& path) {
    return wideToUtf8_or_empty(path.native());
}

std::filesystem::path utf8ToPath(std::string_view utf8) {
    return std::filesystem::path(utf8ToWide_or_empty(utf8));
}

std::string toLowercaseAscii(std::string_view str) {
    std::string result(str);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::wstring toLowercaseAscii(std::wstring_view str) {
    std::wstring result(str);
    std::ranges::transform(result, result.begin(), [](wchar_t c) {
        if (c >= L'A' && c <= L'Z') {
            return static_cast<wchar_t>(c + (L'a' - L'A'));
        }
        return c;
    });
    return result;
}

std::string_view trim(std::string_view str) {
    // Find first non-whitespace
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    if (start == str.size()) {
        return {};
    }

    // Find last non-whitespace
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    return str.substr(start, end - start);
}

std::wstring_view trim(std::wstring_view str) {
    // Find first non-whitespace
    size_t start = 0;
    while (start < str.size() && std::iswspace(str[start])) {
        ++start;
    }

    if (start == str.size()) {
        return {};
    }

    // Find last non-whitespace
    size_t end = str.size();
    while (end > start && std::iswspace(str[end - 1])) {
        --end;
    }

    return str.substr(start, end - start);
}

bool startsWith(std::string_view str, std::string_view prefix) {
    return str.starts_with(prefix);
}

bool endsWith(std::string_view str, std::string_view suffix) {
    return str.ends_with(suffix);
}

bool startsWithIcase(std::string_view str, std::string_view prefix) {
    if (str.size() < prefix.size()) {
        return false;
    }
    return std::ranges::equal(
        str.substr(0, prefix.size()), prefix,
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

bool endsWithIcase(std::string_view str, std::string_view suffix) {
    if (str.size() < suffix.size()) {
        return false;
    }
    return std::ranges::equal(
        str.substr(str.size() - suffix.size()), suffix,
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

}  // namespace nive
