/// @file settings_manager.hpp
/// @brief Settings persistence and validation
///
/// Handles loading and saving settings from TOML files.
/// Requires toml++ library.

#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "settings.hpp"

namespace nive::config {

/// @brief Configuration errors
enum class ConfigError {
    FileNotFound,
    ParseError,
    ValidationError,
    IoError,
};

/// @brief Get string representation of ConfigError
[[nodiscard]] constexpr std::string_view to_string(ConfigError error) noexcept {
    switch (error) {
    case ConfigError::FileNotFound:
        return "Configuration file not found";
    case ConfigError::ParseError:
        return "Failed to parse configuration file";
    case ConfigError::ValidationError:
        return "Configuration validation failed";
    case ConfigError::IoError:
        return "I/O error";
    }
    return "Unknown configuration error";
}

/// @brief Validation result with errors and warnings
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// @brief Settings manager for loading/saving configuration
class SettingsManager {
public:
    /// @brief Load settings from default location
    /// @return Settings or error
    [[nodiscard]] static std::expected<Settings, ConfigError> load();

    /// @brief Load settings from specific path
    /// @param path Path to settings file
    /// @return Settings or error
    [[nodiscard]] static std::expected<Settings, ConfigError>
    loadFrom(const std::filesystem::path& path);

    /// @brief Load settings or return defaults if not found
    /// @return Settings (defaults on error)
    [[nodiscard]] static Settings loadOrDefault();

    /// @brief Save settings to default location
    /// @param settings Settings to save
    /// @return Success or error
    [[nodiscard]] static std::expected<void, ConfigError> save(const Settings& settings);

    /// @brief Save settings to specific path
    /// @param settings Settings to save
    /// @param path Path to save to
    /// @return Success or error
    [[nodiscard]] static std::expected<void, ConfigError> saveTo(const Settings& settings,
                                                                 const std::filesystem::path& path);

    /// @brief Validate settings
    /// @param settings Settings to validate
    /// @return Validation result with errors and warnings
    [[nodiscard]] static ValidationResult validate(const Settings& settings);

    /// @brief Get default settings file path
    /// @return Path to settings.toml
    [[nodiscard]] static std::filesystem::path defaultPath();
};

/// @brief Get cache directory based on settings
/// @param settings Current settings
/// @return Path to cache directory
[[nodiscard]] std::filesystem::path getCachePath(const Settings& settings);

}  // namespace nive::config
