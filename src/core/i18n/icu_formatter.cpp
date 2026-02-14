/// @file icu_formatter.cpp
/// @brief ICU MessageFormat wrapper implementation

#include "icu_formatter.hpp"

#include <Windows.h>

#include <icu.h>

#include <vector>

namespace nive::i18n {

namespace {

// Convert UTF-8 to UChar (char16_t, compatible with wchar_t on Windows)
std::vector<UChar> toUChar(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    if (size_needed <= 0) {
        return {};
    }
    std::vector<UChar> result(static_cast<size_t>(size_needed));
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        reinterpret_cast<wchar_t*>(result.data()), size_needed);
    return result;
}

// Convert UTF-8 to wstring (fallback output)
std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    if (size_needed <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size_needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(),
                        size_needed);
    return result;
}

}  // namespace

std::wstring icuFormat(std::string_view pattern, std::string_view locale,
                       std::initializer_list<std::pair<std::string, int64_t>> args) {
    // Convert pattern to UChar
    auto u_pattern = toUChar(pattern);
    if (u_pattern.empty()) {
        return toWide(pattern);
    }

    // Open the message format
    UErrorCode status = U_ZERO_ERROR;
    std::string locale_str(locale);
    UMessageFormat* fmt =
        umsg_open(u_pattern.data(), static_cast<int32_t>(u_pattern.size()),
                  locale_str.c_str(), nullptr, &status);
    if (U_FAILURE(status) || !fmt) {
        return toWide(pattern);
    }

    // Build argument arrays.
    // ICU umsg_format uses positional args (arg0, arg1, ...).
    // For named patterns like "{count, plural, ...}", the argument names in the pattern
    // map to positional indices in order of first appearance.
    // Since our patterns typically use a single named arg, we pass all values positionally.
    std::vector<int64_t> arg_values;
    for (const auto& [name, value] : args) {
        arg_values.push_back(value);
    }

    // Format the message
    // umsg_format is variadic; for simplicity with dynamic args, use a fixed buffer approach
    UChar result_buf[1024] = {};
    int32_t result_len = 0;
    status = U_ZERO_ERROR;

    // Use umsg_vformat with argument list based on number of args
    if (arg_values.empty()) {
        result_len = umsg_format(fmt, result_buf, 1024, &status);
    } else if (arg_values.size() == 1) {
        result_len =
            umsg_format(fmt, result_buf, 1024, &status, static_cast<double>(arg_values[0]));
    } else if (arg_values.size() == 2) {
        result_len = umsg_format(fmt, result_buf, 1024, &status,
                                 static_cast<double>(arg_values[0]),
                                 static_cast<double>(arg_values[1]));
    } else if (arg_values.size() == 3) {
        result_len = umsg_format(fmt, result_buf, 1024, &status,
                                 static_cast<double>(arg_values[0]),
                                 static_cast<double>(arg_values[1]),
                                 static_cast<double>(arg_values[2]));
    }

    umsg_close(fmt);

    if (U_FAILURE(status) || result_len <= 0) {
        return toWide(pattern);
    }

    // UChar -> wstring (both are 16-bit UTF-16 on Windows)
    static_assert(sizeof(UChar) == sizeof(wchar_t), "UChar must be same size as wchar_t");
    return std::wstring(reinterpret_cast<const wchar_t*>(result_buf),
                        static_cast<size_t>(result_len));
}

}  // namespace nive::i18n
