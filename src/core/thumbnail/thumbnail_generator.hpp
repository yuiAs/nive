/// @file thumbnail_generator.hpp
/// @brief Asynchronous thumbnail generation service
///
/// Manages a pool of worker threads for generating thumbnails
/// from image files.

#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "thumbnail_queue.hpp"
#include "thumbnail_request.hpp"

namespace nive::cache {
class CacheManager;
}

namespace nive::image {
class WicDecoder;
}

namespace nive::thumbnail {

/// @brief Configuration for thumbnail generator
struct GeneratorConfig {
    uint32_t worker_count = 4;  // Number of worker threads
    uint32_t default_thumbnail_size = 256;
    size_t max_queue_size = 1000;  // Maximum pending requests
};

/// @brief Statistics for thumbnail generation
struct GeneratorStats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> completed_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> cancelled_requests{0};
    std::atomic<uint64_t> total_processing_time_ms{0};
};

/// @brief Asynchronous thumbnail generation service
///
/// Manages thumbnail generation using dedicated worker threads and a priority queue.
/// Thread-safe for all public methods.
///
/// Async pattern: Queue + Worker Threads
/// Uses dedicated std::jthread workers (not the global ThreadPool) because:
/// - Priority queue allows reordering pending requests (visible-first thumbnails)
/// - Dedicated workers keep CPU-bound decoding separate from general tasks
/// - Worker count is tunable independently of the global pool
class ThumbnailGenerator {
public:
    /// @brief Construct generator with configuration
    explicit ThumbnailGenerator(const GeneratorConfig& config = {});

    /// @brief Destructor - stops all workers
    ~ThumbnailGenerator();

    // Non-copyable, non-movable
    ThumbnailGenerator(const ThumbnailGenerator&) = delete;
    ThumbnailGenerator& operator=(const ThumbnailGenerator&) = delete;
    ThumbnailGenerator(ThumbnailGenerator&&) = delete;
    ThumbnailGenerator& operator=(ThumbnailGenerator&&) = delete;

    /// @brief Start the generator (starts worker threads)
    void start();

    /// @brief Stop the generator (waits for workers to finish)
    void stop();

    /// @brief Check if generator is running
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Request thumbnail generation for a file
    /// @param path Path to image file
    /// @param callback Callback when thumbnail is ready
    /// @param priority Request priority
    /// @param size Thumbnail size (max dimension)
    /// @return Request ID for cancellation
    [[nodiscard]] RequestId request(const std::filesystem::path& path, ThumbnailCallback callback,
                                    Priority priority = Priority::Normal,
                                    uint32_t size = 0  // 0 = use default
    );

    /// @brief Request thumbnail generation from memory data
    /// @param virtual_path Virtual path for identification
    /// @param data Image data in memory
    /// @param callback Callback when thumbnail is ready
    /// @param priority Request priority
    /// @param size Thumbnail size (max dimension)
    /// @return Request ID for cancellation
    [[nodiscard]] RequestId requestFromMemory(const std::filesystem::path& virtual_path,
                                              std::vector<uint8_t> data, ThumbnailCallback callback,
                                              Priority priority = Priority::Normal,
                                              uint32_t size = 0);

    /// @brief Cancel a pending request
    /// @param id Request ID to cancel
    /// @return true if request was found and cancelled
    bool cancel(RequestId id);

    /// @brief Cancel all requests for a path
    /// @param path Path to match
    /// @return Number of cancelled requests
    size_t cancelByPath(const std::filesystem::path& path);

    /// @brief Cancel all pending requests
    /// @return Number of cancelled requests
    size_t cancelAll();

    /// @brief Update priority of a pending request
    /// @param id Request ID
    /// @param new_priority New priority
    /// @return true if request was found and updated
    bool updatePriority(RequestId id, Priority new_priority);

    /// @brief Get number of pending requests
    [[nodiscard]] size_t pendingCount() const;

    /// @brief Get generator statistics
    [[nodiscard]] const GeneratorStats& stats() const noexcept;

    /// @brief Reset statistics
    void resetStats();

    /// @brief Set cache manager for thumbnail caching
    /// @param cache Pointer to cache manager (can be nullptr to disable caching)
    void setCacheManager(cache::CacheManager* cache) noexcept;

private:
    /// @brief Worker thread function
    void workerThread(std::stop_token stop_token);

    /// @brief Process a single thumbnail request
    void processRequest(ThumbnailRequest& request, image::WicDecoder& decoder);

    /// @brief Generate next unique request ID
    [[nodiscard]] RequestId nextRequestId();

    GeneratorConfig config_;
    ThumbnailQueue queue_;
    std::vector<std::jthread> workers_;
    GeneratorStats stats_;
    std::atomic<RequestId> next_id_{1};
    std::atomic<bool> running_{false};
    cache::CacheManager* cache_ = nullptr;
};

}  // namespace nive::thumbnail
