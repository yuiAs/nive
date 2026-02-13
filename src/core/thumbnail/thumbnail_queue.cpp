/// @file thumbnail_queue.cpp
/// @brief Thread-safe priority queue implementation

#include "thumbnail_queue.hpp"

#include <algorithm>

#include "../util/logger.hpp"

namespace nive::thumbnail {

void ThumbnailQueue::push(ThumbnailRequest request) {
    auto id = request.id;
    auto path = request.source.path.string();
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            LOG_WARN("ThumbnailQueue::push: queue is stopped, ignoring request {}", id);
            return;
        }
        queue_.push(std::move(request));
        LOG_TRACE("ThumbnailQueue::push: added request {}, queue_size={}, path={}", id,
                  queue_.size(), path);
    }
    cv_.notify_one();
}

void ThumbnailQueue::pushBatch(std::vector<ThumbnailRequest> requests) {
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            return;
        }
        for (auto& req : requests) {
            queue_.push(std::move(req));
        }
    }
    cv_.notify_all();
}

std::optional<ThumbnailRequest> ThumbnailQueue::pop() {
    std::unique_lock lock(mutex_);

    LOG_TRACE("ThumbnailQueue::pop: waiting, queue_size={}, stopped={}", queue_.size(), stopped_);
    cv_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    LOG_TRACE("ThumbnailQueue::pop: woke up, queue_size={}, stopped={}", queue_.size(), stopped_);

    // Return immediately when stopped (don't process remaining items)
    if (stopped_) {
        LOG_DEBUG("ThumbnailQueue::pop: returning nullopt due to stopped");
        return std::nullopt;
    }

    // Skip cancelled requests
    while (!queue_.empty()) {
        auto request = std::move(const_cast<ThumbnailRequest&>(queue_.top()));
        queue_.pop();

        if (cancelled_.find(request.id) == cancelled_.end()) {
            LOG_TRACE("ThumbnailQueue::pop: returning request {}, remaining={}", request.id,
                      queue_.size());
            return request;
        }
        LOG_DEBUG("ThumbnailQueue::pop: skipping cancelled request {}", request.id);
        // Request was cancelled, try next one
    }

    LOG_DEBUG("ThumbnailQueue::pop: queue empty after skipping cancelled, returning nullopt");
    return std::nullopt;
}

std::optional<ThumbnailRequest> ThumbnailQueue::tryPop() {
    std::lock_guard lock(mutex_);

    while (!queue_.empty()) {
        auto request = std::move(const_cast<ThumbnailRequest&>(queue_.top()));
        queue_.pop();

        if (cancelled_.find(request.id) == cancelled_.end()) {
            return request;
        }
        // Request was cancelled, try next one
    }

    return std::nullopt;
}

bool ThumbnailQueue::cancel(RequestId id) {
    std::lock_guard lock(mutex_);
    auto [_, inserted] = cancelled_.insert(id);
    return inserted;
}

size_t ThumbnailQueue::cancelByPath(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);

    // We need to rebuild the queue, collecting non-matching requests
    std::vector<ThumbnailRequest> remaining;
    size_t cancelled_count = 0;

    while (!queue_.empty()) {
        auto request = std::move(const_cast<ThumbnailRequest&>(queue_.top()));
        queue_.pop();

        if (request.source.path == path) {
            cancelled_.insert(request.id);
            ++cancelled_count;
        } else {
            remaining.push_back(std::move(request));
        }
    }

    // Rebuild queue
    for (auto& req : remaining) {
        queue_.push(std::move(req));
    }

    return cancelled_count;
}

size_t ThumbnailQueue::cancelAll() {
    std::lock_guard lock(mutex_);
    size_t count = queue_.size();
    LOG_DEBUG("ThumbnailQueue::cancelAll: cancelling {} requests", count);

    while (!queue_.empty()) {
        cancelled_.insert(queue_.top().id);
        queue_.pop();
    }

    return count;
}

bool ThumbnailQueue::isCancelled(RequestId id) const {
    std::lock_guard lock(mutex_);
    return cancelled_.find(id) != cancelled_.end();
}

void ThumbnailQueue::clearCancelled(RequestId id) {
    std::lock_guard lock(mutex_);
    cancelled_.erase(id);
}

bool ThumbnailQueue::updatePriority(RequestId id, Priority new_priority) {
    std::lock_guard lock(mutex_);

    // Find and update the request, then rebuild queue
    std::vector<ThumbnailRequest> requests;
    bool found = false;

    while (!queue_.empty()) {
        auto request = std::move(const_cast<ThumbnailRequest&>(queue_.top()));
        queue_.pop();

        if (request.id == id) {
            request.priority = new_priority;
            found = true;
        }
        requests.push_back(std::move(request));
    }

    // Rebuild queue
    for (auto& req : requests) {
        queue_.push(std::move(req));
    }

    return found;
}

size_t ThumbnailQueue::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

bool ThumbnailQueue::empty() const {
    std::lock_guard lock(mutex_);
    return queue_.empty();
}

void ThumbnailQueue::stop() {
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

bool ThumbnailQueue::isStopped() const {
    std::lock_guard lock(mutex_);
    return stopped_;
}

void ThumbnailQueue::restart() {
    std::lock_guard lock(mutex_);
    stopped_ = false;
    cancelled_.clear();
}

void ThumbnailQueue::rebuildQueue() {
    // Called with lock held
    std::vector<ThumbnailRequest> requests;
    while (!queue_.empty()) {
        requests.push_back(std::move(const_cast<ThumbnailRequest&>(queue_.top())));
        queue_.pop();
    }
    for (auto& req : requests) {
        queue_.push(std::move(req));
    }
}

}  // namespace nive::thumbnail
