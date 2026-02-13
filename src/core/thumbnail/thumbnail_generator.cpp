/// @file thumbnail_generator.cpp
/// @brief Thumbnail generator implementation

#include "thumbnail_generator.hpp"
#include <Windows.h>

#include <objbase.h>

#include <chrono>

#include "../cache/cache_manager.hpp"
#include "../image/image_scaler.hpp"
#include "../image/wic_decoder.hpp"
#include "../util/logger.hpp"

namespace nive::thumbnail {

ThumbnailGenerator::ThumbnailGenerator(const GeneratorConfig& config) : config_(config) {
}

ThumbnailGenerator::~ThumbnailGenerator() {
    stop();
}

void ThumbnailGenerator::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    queue_.restart();
    workers_.reserve(config_.worker_count);

    for (uint32_t i = 0; i < config_.worker_count; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) { workerThread(stop_token); });
    }
}

void ThumbnailGenerator::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }

    queue_.stop();

    // Request stop for all workers
    for (auto& worker : workers_) {
        worker.request_stop();
    }

    // Wait for workers to finish
    workers_.clear();
}

bool ThumbnailGenerator::isRunning() const noexcept {
    return running_.load();
}

RequestId ThumbnailGenerator::request(const std::filesystem::path& path, ThumbnailCallback callback,
                                      Priority priority, uint32_t size) {
    auto id = nextRequestId();

    ThumbnailRequest req{
        .id = id,
        .source = ThumbnailSource::from_file(path),
        .target_size = size > 0 ? size : config_.default_thumbnail_size,
        .priority = priority,
        .callback = std::move(callback),
    };

    stats_.total_requests.fetch_add(1, std::memory_order_relaxed);
    queue_.push(std::move(req));

    return id;
}

RequestId ThumbnailGenerator::requestFromMemory(const std::filesystem::path& virtual_path,
                                                std::vector<uint8_t> data,
                                                ThumbnailCallback callback, Priority priority,
                                                uint32_t size) {
    auto id = nextRequestId();

    ThumbnailRequest req{
        .id = id,
        .source = ThumbnailSource::from_memory(virtual_path, std::move(data)),
        .target_size = size > 0 ? size : config_.default_thumbnail_size,
        .priority = priority,
        .callback = std::move(callback),
    };

    stats_.total_requests.fetch_add(1, std::memory_order_relaxed);
    queue_.push(std::move(req));

    return id;
}

bool ThumbnailGenerator::cancel(RequestId id) {
    if (queue_.cancel(id)) {
        stats_.cancelled_requests.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

size_t ThumbnailGenerator::cancelByPath(const std::filesystem::path& path) {
    size_t count = queue_.cancelByPath(path);
    stats_.cancelled_requests.fetch_add(count, std::memory_order_relaxed);
    return count;
}

size_t ThumbnailGenerator::cancelAll() {
    size_t count = queue_.cancelAll();
    stats_.cancelled_requests.fetch_add(count, std::memory_order_relaxed);
    return count;
}

bool ThumbnailGenerator::updatePriority(RequestId id, Priority new_priority) {
    return queue_.updatePriority(id, new_priority);
}

size_t ThumbnailGenerator::pendingCount() const {
    return queue_.size();
}

const GeneratorStats& ThumbnailGenerator::stats() const noexcept {
    return stats_;
}

void ThumbnailGenerator::resetStats() {
    stats_.total_requests.store(0, std::memory_order_relaxed);
    stats_.completed_requests.store(0, std::memory_order_relaxed);
    stats_.failed_requests.store(0, std::memory_order_relaxed);
    stats_.cancelled_requests.store(0, std::memory_order_relaxed);
    stats_.total_processing_time_ms.store(0, std::memory_order_relaxed);
}

void ThumbnailGenerator::setCacheManager(cache::CacheManager* cache) noexcept {
    cache_ = cache;
}

void ThumbnailGenerator::workerThread(std::stop_token stop_token) {
    auto thread_id = GetCurrentThreadId();
    LOG_DEBUG("workerThread[{}]: starting", thread_id);

    // Initialize COM for this thread (required for WIC)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool com_initialized = SUCCEEDED(hr);
    if (!com_initialized) {
        LOG_ERROR("workerThread[{}]: COM initialization failed: 0x{:08X}", thread_id,
                  static_cast<unsigned>(hr));
    }

    // Create decoder in this scope so it's destroyed before CoUninitialize
    {
        image::WicDecoder decoder;

        while (!stop_token.stop_requested()) {
            LOG_TRACE("workerThread[{}]: waiting for request, queue_size={}", thread_id,
                      queue_.size());
            auto request_opt = queue_.pop();
            if (!request_opt) {
                // Queue stopped or empty - check if we should exit
                LOG_DEBUG("workerThread[{}]: pop returned nullopt, stopped={}", thread_id,
                          queue_.isStopped());
                if (queue_.isStopped()) {
                    break;
                }
                continue;
            }

            auto& request = *request_opt;
            LOG_DEBUG("workerThread[{}]: got request id={}, path={}", thread_id, request.id,
                      request.source.path.string());

            // Check if cancelled while waiting
            if (queue_.isCancelled(request.id)) {
                LOG_DEBUG("workerThread[{}]: request {} was cancelled", thread_id, request.id);
                queue_.clearCancelled(request.id);
                continue;
            }

            // Check stop again before processing
            if (stop_token.stop_requested() || queue_.isStopped()) {
                LOG_DEBUG("workerThread[{}]: stop requested before processing", thread_id);
                break;
            }

            LOG_DEBUG("workerThread[{}]: processing request {}", thread_id, request.id);
            processRequest(request, decoder);
            LOG_DEBUG("workerThread[{}]: completed request {}", thread_id, request.id);
        }
    }  // decoder destroyed here

    // Clean up COM
    if (com_initialized) {
        CoUninitialize();
    }
    LOG_DEBUG("workerThread[{}]: exiting", thread_id);
}

void ThumbnailGenerator::processRequest(ThumbnailRequest& request, image::WicDecoder& decoder) {
    auto start_time = std::chrono::steady_clock::now();

    ThumbnailResult result{
        .path = request.source.path,
        .thumbnail = std::nullopt,
        .error = std::nullopt,
    };

    // Check cache first (only for file-based sources, not memory)
    bool is_file_source = !request.source.memory_data.has_value();
    if (cache_ && is_file_source) {
        auto cached = cache_->getThumbnail(request.source.path);
        if (cached) {
            result.thumbnail = std::move(*cached);
            stats_.completed_requests.fetch_add(1, std::memory_order_relaxed);

            // Update timing stats
            auto end_time = std::chrono::steady_clock::now();
            auto duration_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                    .count();
            stats_.total_processing_time_ms.fetch_add(static_cast<uint64_t>(duration_ms),
                                                      std::memory_order_relaxed);

            // Invoke callback
            if (request.callback && !queue_.isStopped()) {
                try {
                    request.callback(std::move(result));
                } catch (...) {
                    // Ignore callback exceptions
                }
            }
            return;
        }
    }

    // Decode image
    std::expected<image::DecodedImage, image::DecodeError> decode_result;

    if (request.source.memory_data) {
        // Decode from memory
        decode_result = decoder.decodeFromMemory(*request.source.memory_data);
    } else {
        // Decode from file
        decode_result = decoder.decode(request.source.path);
    }

    if (!decode_result) {
        result.error = std::string(image::to_string(decode_result.error()));
        stats_.failed_requests.fetch_add(1, std::memory_order_relaxed);
    } else {
        // Store original dimensions for cache
        uint32_t original_width = decode_result->width();
        uint32_t original_height = decode_result->height();

        // Generate thumbnail
        auto thumb_result = image::generateThumbnail(*decode_result, request.target_size);

        if (thumb_result) {
            // Save to cache before moving (only for file-based sources)
            if (cache_ && is_file_source) {
                // Use synchronous putThumbnail; worker threads can handle the blocking
                auto cache_result = cache_->putThumbnail(request.source.path, *thumb_result,
                                                         original_width, original_height);
                // Cache failures are not critical, just log them
                if (!cache_result) {
                    LOG_DEBUG("Failed to cache thumbnail for {}", request.source.path.string());
                }
            }

            result.thumbnail = std::move(*thumb_result);
            stats_.completed_requests.fetch_add(1, std::memory_order_relaxed);
        } else {
            result.error = std::string(image::to_string(thumb_result.error()));
            stats_.failed_requests.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Update timing stats
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    stats_.total_processing_time_ms.fetch_add(static_cast<uint64_t>(duration_ms),
                                              std::memory_order_relaxed);

    // Invoke callback only if not stopped (avoid posting to destroyed window)
    if (request.callback && !queue_.isStopped()) {
        try {
            request.callback(std::move(result));
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

RequestId ThumbnailGenerator::nextRequestId() {
    return next_id_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace nive::thumbnail
