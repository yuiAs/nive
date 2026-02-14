/// @file locale_detector.hpp
/// @brief System locale detection using Windows built-in ICU

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nive::i18n {

/// @brief Detect the system locale language tag (e.g. "en", "ja")
/// Uses ICU uloc_getDefault() / uloc_getLanguage() to extract the language subtag.
/// Falls back to "en" if detection fails.
[[nodiscard]] std::string detectSystemLocale();

/// @brief Resolve the best available locale from the given language setting.
/// @param language Language setting ("auto" for system detection, or explicit tag like "en", "ja")
/// @param locale_dir Directory containing *.toml locale files
/// @return Resolved language tag with a matching .toml file, or "en" as final fallback
[[nodiscard]] std::string resolveLocale(std::string_view language,
                                        const std::filesystem::path& locale_dir);

/// @brief List all available locale tags by scanning for *.toml files
/// @param locale_dir Directory containing locale files
/// @return Sorted vector of language tags (stems of .toml files)
[[nodiscard]] std::vector<std::string> listAvailableLocales(
    const std::filesystem::path& locale_dir);

}  // namespace nive::i18n
