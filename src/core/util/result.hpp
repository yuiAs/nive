/// @file result.hpp
/// @brief Error handling utilities using std::expected
///
/// Provides Result<T, E> type alias and helper functions for
/// consistent error handling throughout the codebase.

#pragma once

#include <expected>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace nive {

/// @brief Result type alias for functions that can fail
/// @tparam T Success value type
/// @tparam E Error type
template <typename T, typename E>
using Result = std::expected<T, E>;

/// @brief Result type for void-returning functions that can fail
/// @tparam E Error type
template <typename E>
using VoidResult = std::expected<void, E>;

/// @brief Create an error result
/// @param error The error value
/// @return std::unexpected containing the error
template <typename E>
[[nodiscard]] constexpr auto makeError(E error) {
    return std::unexpected(error);
}

/// @brief Error information with source location
/// @tparam ErrorCode The error code enum type
template <typename ErrorCode>
class ErrorInfo {
public:
    constexpr ErrorInfo(ErrorCode code, std::string message = "",
                        std::source_location location = std::source_location::current())
        : code_(code), message_(std::move(message)), location_(location) {}

    [[nodiscard]] constexpr ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] const std::source_location& location() const noexcept { return location_; }

    /// @brief Format error information as string
    [[nodiscard]] std::string format() const {
        if (message_.empty()) {
            return std::format("{} at {}:{}", to_string(code_), location_.file_name(),
                               location_.line());
        }
        return std::format("{}: {} at {}:{}", to_string(code_), message_, location_.file_name(),
                           location_.line());
    }

private:
    ErrorCode code_;
    std::string message_;
    std::source_location location_;
};

/// @brief Log and return an error (for chaining)
///
/// Usage:
///   return logAndReturn(SomeError::Failed, "context info");
template <typename E>
[[nodiscard]] auto
logAndReturn(E error, [[maybe_unused]] std::string_view context = "",
             [[maybe_unused]] std::source_location loc = std::source_location::current()) {
    // TODO: Integrate with spdlog when available
    // spdlog::error("[{}:{}] {}: {}", loc.file_name(), loc.line(), context, to_string(error));
    return std::unexpected(error);
}

}  // namespace nive

/// @brief TRY macro for error propagation
///
/// Evaluates the expression and returns early if it's an error.
/// Otherwise, extracts the value.
///
/// Usage:
///   auto value = NIVE_TRY(some_operation());
///
/// Note: This macro uses a GCC/MSVC extension (statement expressions).
/// For standard-compliant code, use explicit error checking.
#ifdef _MSC_VER
   // MSVC doesn't support statement expressions well, provide alternative pattern
    #define NIVE_TRY(expr)                                                                         \
        [&]() -> decltype(auto) {                                                                  \
            auto&& _result = (expr);                                                               \
            if (!_result) {                                                                        \
                return std::unexpected(_result.error());                                           \
            }                                                                                      \
            if constexpr (!std::is_void_v<                                                         \
                              typename std::remove_cvref_t<decltype(_result)>::value_type>) {      \
                return *std::move(_result);                                                        \
            }                                                                                      \
        }()
#else
    #define NIVE_TRY(expr)                                                                         \
        ({                                                                                         \
            auto&& _result = (expr);                                                               \
            if (!_result) {                                                                        \
                return std::unexpected(_result.error());                                           \
            }                                                                                      \
            std::move(*_result);                                                                   \
        })
#endif

/// @brief Propagate error without extracting value
///
/// Usage:
///   NIVE_TRY_VOID(some_void_operation());
#define NIVE_TRY_VOID(expr)                                                                        \
    do {                                                                                           \
        auto&& _result = (expr);                                                                   \
        if (!_result) {                                                                            \
            return std::unexpected(_result.error());                                               \
        }                                                                                          \
    } while (0)
