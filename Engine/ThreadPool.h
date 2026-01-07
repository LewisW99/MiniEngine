#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstddef>

/// <summary>
/// Fixed-size thread pool for running generic tasks.
/// Later extended by JobSystem for dependency tracking.
/// </summary>
class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
    ~ThreadPool();

    /// <summary>
    /// Submit a task for execution.
    /// </summary>
    void Submit(std::function<void()> task);

    /// <summary>
    /// Returns number of worker threads.
    /// </summary>
    [[nodiscard]] size_t GetThreadCount() const noexcept { return m_Workers.size(); }

    /// <summary>
    /// Returns approximate queue length (for profiling overlay).
    /// </summary>
    [[nodiscard]] size_t GetQueueSize() const;

private:
    void WorkerLoop();

    std::vector<std::thread> m_Workers;
    std::queue<std::function<void()>> m_Tasks;

    mutable std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
    std::atomic<bool> m_Stop{ false };
};
