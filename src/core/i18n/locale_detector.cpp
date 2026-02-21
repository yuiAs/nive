/// @file locale_detector.cpp
/// @brief System locale detection using Windows built-in ICU

#include "locale_detector.hpp"

#include <icu.h>

#include <algorithm>

#include "../util/string_utils.hpp"

namespace nive::i18n {

std::string detectSystemLocale() {
    // Get the default ICU locale (e.g. "en_US", "ja_JP")
    const char* default_locale = uloc_getDefault();
    if (!default_locale || default_locale[0] == '\0') {
        return "en";
    }

    // Extract language subtag (e.g. "en" from "en_US")
    char language[ULOC_LANG_CAPACITY] = {};
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = uloc_getLanguage(default_locale, language, ULOC_LANG_CAPACITY, &status);
    if (U_FAILURE(status) || len <= 0) {
        return "en";
    }

    return std::string(language, static_cast<size_t>(len));
}

std::string resolveLocale(std::string_view language, const std::filesystem::path& locale_dir) {
    std::string tag;

    if (language == "auto" || language.empty()) {
        tag = detectSystemLocale();
    } else {
        tag = std::string(language);
    }

    // Check if locale file exists for the resolved tag
    auto locale_file = locale_dir / (tag + ".toml");
    if (std::filesystem::exists(locale_file)) {
        return tag;
    }

    // Fallback to English
    if (tag != "en") {
        auto en_file = locale_dir / "en.toml";
        if (std::filesystem::exists(en_file)) {
            return "en";
        }
    }

    // No locale file available at all
    return tag;
}

std::vector<std::string> listAvailableLocales(const std::filesystem::path& locale_dir) {
    std::vector<std::string> locales;

    std::error_code ec;
    if (!std::filesystem::exists(locale_dir, ec)) {
        return locales;
    }

    for (const auto& entry : std::filesystem::directory_iterator(locale_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") {
            locales.push_back(pathToUtf8(entry.path().stem()));
        }
    }

    std::sort(locales.begin(), locales.end());
    return locales;
}

}  // namespace nive::i18n
