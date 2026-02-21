/// @file message_store.cpp
/// @brief TOML-based translation message storage implementation

#include "message_store.hpp"

#include <Windows.h>

#include "../util/string_utils.hpp"

#ifdef NIVE_HAS_TOMLPLUSPLUS
    #include <toml++/toml.hpp>
#endif

namespace nive::i18n {

namespace {

// Convert UTF-8 string to UTF-16 wide string
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

#ifdef NIVE_HAS_TOMLPLUSPLUS

// Recursively flatten TOML tables into dot-separated keys
void flattenTable(const toml::table& table, const std::string& prefix,
                  std::unordered_map<std::string, std::wstring>& translations,
                  std::unordered_map<std::string, std::string>& patterns) {
    for (const auto& [key, value] : table) {
        std::string full_key = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);

        if (auto* sub_table = value.as_table()) {
            flattenTable(*sub_table, full_key, translations, patterns);
        } else if (auto* str = value.as_string()) {
            const std::string& utf8_value = str->get();
            translations[full_key] = toWide(utf8_value);
            patterns[full_key] = utf8_value;
        }
    }
}

#endif  // NIVE_HAS_TOMLPLUSPLUS

}  // namespace

std::expected<MessageStore, MessageStoreError> MessageStore::load(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(MessageStoreError::FileNotFound);
    }

#ifdef NIVE_HAS_TOMLPLUSPLUS
    try {
        auto table = toml::parse_file(pathToUtf8(path));

        MessageStore store;
        flattenTable(table, "", store.translations_, store.patterns_);
        return store;
    } catch (const toml::parse_error&) {
        return std::unexpected(MessageStoreError::ParseError);
    }
#else
    // Without toml++, return empty store (keys will display as-is)
    return MessageStore{};
#endif
}

const std::wstring* MessageStore::find(std::string_view key) const {
    auto it = translations_.find(std::string(key));
    if (it != translations_.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::string* MessageStore::findPattern(std::string_view key) const {
    auto it = patterns_.find(std::string(key));
    if (it != patterns_.end()) {
        return &it->second;
    }
    return nullptr;
}

}  // namespace nive::i18n
