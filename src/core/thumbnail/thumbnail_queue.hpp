/// @file thumbnail_queue.hpp
/// @brief Thread-safe priority queue for thumbnail requests

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

#include "thumbnail_request.hpp"

namespace nive::thumbnail {

/// @brief Thread-safe priority queue for thumbnail requests
///
/// Supports:
/// - Priority-based ordering
/// - Request cancellation
/// - Batch operations
/// - Graceful shutdown
class ThumbnailQueue {
public:
    ThumbnailQueue() = default;
    ~ThumbnailQueue() = default;

    // Non-copyable, non-movable
    ThumbnailQueue(const ThumbnailQueue&) = delete;
    ThumbnailQueue& operator=(const ThumbnailQueue&) = delete;
    ThumbnailQueue(ThumbnailQueue&&) = delete;
    ThumbnailQueue& operator=(ThumbnailQueue&&) = delete;

    /// @brief Push a request to the queue
    /// @param request Request to add
    void push(ThumbnailRequest request);

    /// @brief Push multiple requests to the queue
    /// @param requests Requests to add
    void pushBatch(std::vector<ThumbnailRequest> requests);

    /// @brief Pop highest priority request from the queue
    /// @return Request or nullopt if queue is empty/stopped
    ///
    /// Blocks until a request is available or the queue is stopped.
    [[nodiscard]] std::optional<ThumbnailRequest> pop();

    /// @brief Try to pop a request without blocking
    /// @return Request or nullopt if queue is empty
    [[nodiscard]] std::optional<ThumbnailRequest> tryPop();

    /// @brief Cancel a specific request by ID
    /// @param id Request ID to cancel
    /// @return true if request was found and cancelled
    bool cancel(RequestId id);

    /// @brief Cancel all requests matching a path
    /// @param path Path to match
    /// @return Number of cancelled requests
    size_t cancelByPath(const std::filesystem::path& path);

    /// @brief Cancel all pending requests
    /// @return Number of cancelled requests
    size_t cancelAll();

    /// @brief Check if a request ID is cancelled
    /// @param id Request ID to check
    [[nodiscard]] bool isCancelled(RequestId id) const;

    /// @brief Clear the cancelled set for a request ID
    /// @param id Request ID to clear
    void clearCancelled(RequestId id);

    /// @brief Update priority of a request
    /// @param id Request ID
    /// @param new_priority New priority
    /// @return true if request was found and updated
    bool updatePriority(RequestId id, Priority new_priority);

    /// @brief Get number of pending requests
    [[nodiscard]] size_t size() const;

    /// @brief Check if queue is empty
    [[nodiscard]] bool empty() const;

    /// @brief Stop the queue (unblocks waiting threads)
    void stop();

    /// @brief Check if queue is stopped
    [[nodiscard]] bool isStopped() const;

    /// @brief Restart the queue after stopping
    void restart();

private:
    // Rebuild priority queue after modification
    void rebuildQueue();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<ThumbnailRequest> queue_;
    std::unordered_set<RequestId> cancelled_;
    bool stopped_ = false;
};

}  // namespace nive::thumbnail
