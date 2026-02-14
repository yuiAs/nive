/// @file i18n.cpp
/// @brief Internationalization singleton state and dispatch

#include "i18n.hpp"

#include <Windows.h>

#include <shared_mutex>

#include "icu_formatter.hpp"
#include "locale_detector.hpp"
#include "message_store.hpp"

namespace nive::i18n {

namespace {

// Global i18n state protected by shared_mutex
struct I18nState {
    MessageStore store;
    std::string locale;
    std::filesystem::path locale_dir;
    // Cache for missing key fallback strings (key -> wstring conversion)
    mutable std::unordered_map<std::string, std::wstring> fallback_cache;
    mutable std::shared_mutex mutex;
    bool initialized = false;
};

I18nState& state() {
    static I18nState s;
    return s;
}

// Convert UTF-8 key to wstring for fallback display
std::wstring keyToWide(std::string_view key) {
    if (key.empty()) {
        return {};
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, key.data(), static_cast<int>(key.size()),
                                          nullptr, 0);
    if (size_needed <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size_needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, key.data(), static_cast<int>(key.size()), result.data(),
                        size_needed);
    return result;
}

std::expected<void, I18nError> loadLocale(I18nState& s, std::string_view language) {
    // Resolve locale tag
    std::string tag = resolveLocale(language, s.locale_dir);

    // Load locale file
    auto locale_file = s.locale_dir / (tag + ".toml");
    auto result = MessageStore::load(locale_file);
    if (!result) {
        switch (result.error()) {
        case MessageStoreError::FileNotFound:
            return std::unexpected(I18nError::LocaleFileNotFound);
        case MessageStoreError::ParseError:
            return std::unexpected(I18nError::LocaleParseError);
        }
        return std::unexpected(I18nError::LocaleFileNotFound);
    }

    s.store = std::move(*result);
    s.locale = tag;
    s.fallback_cache.clear();
    s.initialized = true;
    return {};
}

}  // namespace

std::expected<void, I18nError> init(const std::filesystem::path& locale_dir,
                                    std::string_view language) {
    auto& s = state();
    std::unique_lock lock(s.mutex);

    s.locale_dir = locale_dir;
    return loadLocale(s, language);
}

const std::wstring& tr(std::string_view key) {
    auto& s = state();
    std::shared_lock lock(s.mutex);

    // Look up in message store
    if (const auto* translated = s.store.find(key)) {
        return *translated;
    }

    // Fallback: return key as wstring (cached)
    auto it = s.fallback_cache.find(std::string(key));
    if (it != s.fallback_cache.end()) {
        return it->second;
    }

    // Need to insert into cache - upgrade to unique lock
    lock.unlock();
    std::unique_lock write_lock(s.mutex);

    // Double-check after acquiring write lock
    auto it2 = s.fallback_cache.find(std::string(key));
    if (it2 != s.fallback_cache.end()) {
        return it2->second;
    }

    auto [inserted, _] = s.fallback_cache.emplace(std::string(key), keyToWide(key));
    return inserted->second;
}

std::wstring format(std::string_view key,
                    std::initializer_list<std::pair<std::string, int64_t>> args) {
    auto& s = state();
    std::shared_lock lock(s.mutex);

    // Look up the ICU pattern
    const auto* pattern = s.store.findPattern(key);
    if (!pattern) {
        // No pattern found - return key as fallback
        return keyToWide(key);
    }

    std::string locale = s.locale;
    lock.unlock();

    return icuFormat(*pattern, locale, args);
}

std::string_view currentLocale() noexcept {
    auto& s = state();
    std::shared_lock lock(s.mutex);
    return s.locale;
}

std::vector<std::string> availableLocales(const std::filesystem::path& locale_dir) {
    return listAvailableLocales(locale_dir);
}

std::expected<void, I18nError> reload(const std::filesystem::path& locale_dir,
                                      std::string_view language) {
    auto& s = state();
    std::unique_lock lock(s.mutex);

    s.locale_dir = locale_dir;
    return loadLocale(s, language);
}

}  // namespace nive::i18n
