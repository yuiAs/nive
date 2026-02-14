/// @file icu_formatter.hpp
/// @brief ICU MessageFormat wrapper using Windows built-in ICU

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace nive::i18n {

/// @brief Format a message using ICU MessageFormat syntax.
///
/// Uses Windows built-in ICU C API (umsg_open / umsg_format / umsg_close).
/// Supports plural rules, select, and other ICU patterns.
///
/// @param pattern UTF-8 ICU MessageFormat pattern (e.g. "{count, plural, one {# file} other {# files}}")
/// @param locale ICU locale string (e.g. "en")
/// @param args Named integer arguments
/// @return Formatted wide string, or the raw pattern converted to wide on failure
[[nodiscard]] std::wstring icuFormat(
    std::string_view pattern, std::string_view locale,
    std::initializer_list<std::pair<std::string, int64_t>> args);

}  // namespace nive::i18n
