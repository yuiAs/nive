/// @file logger.hpp
/// @brief Application logging utilities using spdlog

#pragma once

// Enable all log levels at compile time
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace nive {

/// @brief Initialize the logging system
/// @param log_file Path to the log file
/// @param console_output Enable console output
/// @return true if initialization succeeded
inline bool init_logging(const std::filesystem::path& log_file, bool console_output = true) {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // File sink
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.string(), true);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);

        // Console sink (optional)
        if (console_output) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::debug);
            sinks.push_back(console_sink);
        }

        // Create and register logger
        auto logger = std::make_shared<spdlog::logger>("nive", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::debug);

        return true;
    } catch (const spdlog::spdlog_ex&) {
        return false;
    }
}

/// @brief Shutdown the logging system
inline void shutdown_logging() {
    spdlog::shutdown();
}

// Convenience macros for logging with source location
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

}  // namespace nive
