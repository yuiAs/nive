/// @file thumbnail_request.hpp
/// @brief Thumbnail generation request structures

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "../image/decoded_image.hpp"

namespace nive::thumbnail {

/// @brief Priority levels for thumbnail requests
enum class Priority : uint8_t {
    Low = 0,        // Background prefetch
    Normal = 1,     // Standard request
    High = 2,       // Currently visible
    Immediate = 3,  // User interaction (e.g., hover)
};

/// @brief Source for thumbnail generation
struct ThumbnailSource {
    std::filesystem::path path;
    std::optional<std::vector<uint8_t>> memory_data;  // For archive contents

    /// @brief Create source from file path
    static ThumbnailSource from_file(const std::filesystem::path& path) {
        return ThumbnailSource{.path = path, .memory_data = std::nullopt};
    }

    /// @brief Create source from memory data (e.g., extracted from archive)
    static ThumbnailSource from_memory(const std::filesystem::path& virtual_path,
                                       std::vector<uint8_t> data) {
        return ThumbnailSource{.path = virtual_path, .memory_data = std::move(data)};
    }
};

/// @brief Result of thumbnail generation
struct ThumbnailResult {
    std::filesystem::path path;
    std::optional<image::DecodedImage> thumbnail;
    std::optional<std::string> error;
    uint32_t original_width = 0;
    uint32_t original_height = 0;

    [[nodiscard]] bool success() const noexcept {
        return thumbnail.has_value() && !error.has_value();
    }
};

/// @brief Callback for thumbnail completion
using ThumbnailCallback = std::function<void(ThumbnailResult)>;

/// @brief Request ID for tracking and cancellation
using RequestId = uint64_t;

/// @brief Thumbnail generation request
struct ThumbnailRequest {
    RequestId id = 0;
    ThumbnailSource source;
    uint32_t target_size = 256;
    Priority priority = Priority::Normal;
    ThumbnailCallback callback;
    std::chrono::steady_clock::time_point created_at = std::chrono::steady_clock::now();

    /// @brief Comparison for priority queue ordering
    ///
    /// Higher priority comes first, then earlier created time.
    [[nodiscard]] bool operator<(const ThumbnailRequest& other) const noexcept {
        if (priority != other.priority) {
            return static_cast<uint8_t>(priority) < static_cast<uint8_t>(other.priority);
        }
        // Earlier created time has higher priority
        return created_at > other.created_at;
    }
};

/// @brief Batch of thumbnail requests
struct ThumbnailBatch {
    std::vector<ThumbnailRequest> requests;
    std::function<void(std::vector<ThumbnailResult>)> batch_callback;
};

}  // namespace nive::thumbnail
