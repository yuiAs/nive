/// @file i18n.hpp
/// @brief Internationalization public API

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nive::i18n {

/// @brief Errors that can occur during i18n operations
enum class I18nError {
    LocaleFileNotFound,
    LocaleParseError,
    IcuError,
};

/// @brief Initialize the i18n system. Call once at startup after settings load.
/// @param locale_dir Path to directory containing *.toml locale files
/// @param language Language setting: "auto" for system detection, or explicit tag (e.g. "en", "ja")
/// @return void on success, or I18nError on failure
[[nodiscard]] std::expected<void, I18nError> init(const std::filesystem::path& locale_dir,
                                                  std::string_view language = "auto");

/// @brief Look up a translated string by key.
/// Returns the translated wide string. On missing key, returns the key itself
/// (converted to UTF-16 and cached).
/// Thread-safe (shared lock).
[[nodiscard]] const std::wstring& tr(std::string_view key);

/// @brief Format a message using ICU MessageFormat with named integer arguments.
/// @param key Translation key whose value is an ICU pattern string
/// @param args Named integer arguments (e.g. {{"count", 5}})
/// @return Formatted wide string. On error, returns the raw pattern or key.
/// Thread-safe (shared lock).
[[nodiscard]] std::wstring format(
    std::string_view key,
    std::initializer_list<std::pair<std::string, int64_t>> args);

/// @brief Get the currently active locale tag (e.g. "en", "ja")
/// Thread-safe.
[[nodiscard]] std::string_view currentLocale() noexcept;

/// @brief List all available locales by scanning the locale directory for *.toml files
/// @param locale_dir Path to directory containing locale files
/// @return Sorted vector of language tags
[[nodiscard]] std::vector<std::string> availableLocales(const std::filesystem::path& locale_dir);

/// @brief Reload translations after a language change.
/// @param locale_dir Path to directory containing *.toml locale files
/// @param language New language setting
/// @return void on success, or I18nError on failure
/// Thread-safe (exclusive lock).
[[nodiscard]] std::expected<void, I18nError> reload(const std::filesystem::path& locale_dir,
                                                    std::string_view language);

}  // namespace nive::i18n
