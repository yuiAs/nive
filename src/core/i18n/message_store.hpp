/// @file message_store.hpp
/// @brief TOML-based translation message storage

#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace nive::i18n {

enum class MessageStoreError {
    FileNotFound,
    ParseError,
};

/// @brief Loads and stores translations from a TOML locale file.
///
/// TOML nested tables are flattened to dot-separated keys:
///   [menu.file] + refresh = "&Refresh\tF5"  ->  key "menu.file.refresh"
///
/// Stores two maps:
///   - translations_: key -> std::wstring (for tr() lookups)
///   - patterns_:     key -> std::string  (UTF-8 ICU MessageFormat patterns)
class MessageStore {
public:
    /// @brief Load translations from a TOML file
    /// @param path Path to the .toml locale file
    /// @return Loaded MessageStore or error
    [[nodiscard]] static std::expected<MessageStore, MessageStoreError> load(
        const std::filesystem::path& path);

    /// @brief Look up a translated wide string by key
    /// @return Pointer to the translated string, or nullptr if not found
    [[nodiscard]] const std::wstring* find(std::string_view key) const;

    /// @brief Look up an ICU pattern string (UTF-8) by key
    /// @return Pointer to the pattern string, or nullptr if not found
    [[nodiscard]] const std::string* findPattern(std::string_view key) const;

    /// @brief Number of loaded translation entries
    [[nodiscard]] size_t size() const noexcept { return translations_.size(); }

    /// @brief Check if store is empty (no translations loaded)
    [[nodiscard]] bool empty() const noexcept { return translations_.empty(); }

private:
    std::unordered_map<std::string, std::wstring> translations_;
    std::unordered_map<std::string, std::string> patterns_;
};

}  // namespace nive::i18n
