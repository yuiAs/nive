/// @file thread_pool.cpp
/// @brief Thread pool implementation

#include "thread_pool.hpp"

#include <format>

namespace nive {

ThreadPool::ThreadPool() : ThreadPool(ThreadPoolConfig{}) {
}

ThreadPool::ThreadPool(ThreadPoolConfig config) : config_(config) {
    size_t num_threads = config_.num_threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 4;  // Fallback
        }
    }

    workers_.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i](std::stop_token stop_token) { workerLoop(stop_token, i); });
    }
}

ThreadPool::~ThreadPool() {
    // Request stop - workers will exit their loops
    stop_source_.request_stop();
    queue_cv_.notify_all();

    // jthreads automatically join on destruction
}

void ThreadPool::workerLoop(std::stop_token stop_token, [[maybe_unused]] size_t worker_id) {
    // Set thread priority
    SetThreadPriority(GetCurrentThread(), static_cast<int>(config_.priority));

    // Set thread name for debugging (Windows 10+)
#if defined(_DEBUG) || defined(DEBUG)
    std::wstring thread_name = std::format(
        L"{}-{}", std::wstring(config_.name_prefix.begin(), config_.name_prefix.end()), worker_id);
    SetThreadDescription(GetCurrentThread(), thread_name.c_str());
#endif

    while (!stop_token.stop_requested()) {
        std::function<void()> task;

        {
            std::unique_lock lock(queue_mutex_);

            // Wait for a task or stop request
            queue_cv_.wait(lock, stop_token, [this] { return !tasks_.empty(); });

            // Check if we should exit
            if (stop_token.stop_requested() && tasks_.empty()) {
                break;
            }

            if (tasks_.empty()) {
                continue;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        if (task) {
            ++active_tasks_;
            task();
            --active_tasks_;
            idle_cv_.notify_all();
        }
    }
}

void ThreadPool::waitIdle() {
    std::unique_lock lock(queue_mutex_);
    idle_cv_.wait(lock, [this] { return tasks_.empty() && active_tasks_.load() == 0; });
}

// Global thread pool singleton
ThreadPool& globalThreadPool() {
    static ThreadPool pool(ThreadPoolConfig{.num_threads = 0,  // Auto-detect
                                            .priority = ThreadPriority::BelowNormal,
                                            .name_prefix = "nive-global"});
    return pool;
}

}  // namespace nive
