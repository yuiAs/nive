/// @file thread_pool.hpp
/// @brief Thread pool implementation using std::jthread and stop_token
///
/// Provides a modern C++20/26 thread pool for background task execution.

#pragma once

#include <Windows.h>

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace nive {

/// @brief Thread priority levels
enum class ThreadPriority {
    Lowest = THREAD_PRIORITY_LOWEST,
    BelowNormal = THREAD_PRIORITY_BELOW_NORMAL,
    Normal = THREAD_PRIORITY_NORMAL,
    AboveNormal = THREAD_PRIORITY_ABOVE_NORMAL,
    Highest = THREAD_PRIORITY_HIGHEST,
};

/// @brief Configuration for thread pool
struct ThreadPoolConfig {
    size_t num_threads = 0;  // 0 = hardware_concurrency()
    ThreadPriority priority = ThreadPriority::BelowNormal;
    std::string_view name_prefix = "nive-worker";
};

/// @brief A modern thread pool using std::jthread
///
/// Features:
/// - Automatic thread count based on hardware_concurrency
/// - Cooperative cancellation via std::stop_token
/// - Returns std::future for task results
/// - Configurable thread priority
///
/// Async pattern: ThreadPool + Future
/// General-purpose pool for fire-and-forget or future-based background tasks.
/// Use this for ad-hoc async work (e.g. directory scanning). For specialized
/// subsystems, prefer dedicated workers: ThumbnailGenerator (priority queue)
/// or CacheDatabase (serialized SQLite I/O).
class ThreadPool {
public:
    /// @brief Create a thread pool with default configuration
    ThreadPool();

    /// @brief Create a thread pool with custom configuration
    explicit ThreadPool(ThreadPoolConfig config);

    /// @brief Destructor - requests stop and waits for all threads
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// @brief Submit a task to the pool
    /// @param task Callable to execute
    /// @return Future for the task result
    template <std::invocable F>
    [[nodiscard]] auto submit(F&& task) -> std::future<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;

        auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(task));

        auto future = packaged->get_future();

        {
            std::lock_guard lock(queue_mutex_);
            if (stop_source_.stop_requested()) {
                // Pool is stopping, don't accept new tasks
                packaged->operator()();  // Execute immediately with potential exception
                return future;
            }

            tasks_.emplace([packaged = std::move(packaged)]() { (*packaged)(); });
        }

        queue_cv_.notify_one();
        return future;
    }

    /// @brief Submit a task that can be cancelled
    /// @param task Callable that takes a stop_token
    /// @return Future for the task result
    template <std::invocable<std::stop_token> F>
    [[nodiscard]] auto submitCancellable(F&& task)
        -> std::future<std::invoke_result_t<F, std::stop_token>> {
        using ReturnType = std::invoke_result_t<F, std::stop_token>;

        auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(
            [task = std::forward<F>(task), stop_token = stop_source_.get_token()]() mutable {
                return task(stop_token);
            });

        auto future = packaged->get_future();

        {
            std::lock_guard lock(queue_mutex_);
            tasks_.emplace([packaged = std::move(packaged)]() { (*packaged)(); });
        }

        queue_cv_.notify_one();
        return future;
    }

    /// @brief Get the number of worker threads
    [[nodiscard]] size_t workerCount() const noexcept { return workers_.size(); }

    /// @brief Get the number of pending tasks
    [[nodiscard]] size_t pendingCount() const {
        std::lock_guard lock(queue_mutex_);
        return tasks_.size();
    }

    /// @brief Check if the pool is stopping
    [[nodiscard]] bool stopping() const noexcept { return stop_source_.stop_requested(); }

    /// @brief Request all tasks to stop (does not wait)
    void requestStop() noexcept {
        stop_source_.request_stop();
        queue_cv_.notify_all();
    }

    /// @brief Wait for all pending tasks to complete
    ///
    /// Note: Does not prevent new tasks from being submitted.
    void waitIdle();

private:
    void workerLoop(std::stop_token stop_token, size_t worker_id);

    std::vector<std::jthread> workers_;
    std::stop_source stop_source_;

    mutable std::mutex queue_mutex_;
    std::condition_variable_any queue_cv_;
    std::queue<std::function<void()>> tasks_;

    std::atomic<size_t> active_tasks_{0};
    std::condition_variable_any idle_cv_;

    ThreadPoolConfig config_;
};

/// @brief Global thread pool for background tasks
///
/// Lazy-initialized singleton.
[[nodiscard]] ThreadPool& globalThreadPool();

}  // namespace nive
