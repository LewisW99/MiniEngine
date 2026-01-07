#include "pch.h"
#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t threadCount)
{
    if (threadCount == 0)
        threadCount = 1;

    m_Workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
    {
        m_Workers.emplace_back([this] { WorkerLoop(); });
    }

    std::cout << "[ThreadPool] Started " << m_Workers.size() << " threads.\n";
}

ThreadPool::~ThreadPool()
{
    m_Stop = true;
    m_Condition.notify_all();

    for (auto& thread : m_Workers)
        if (thread.joinable()) thread.join();

    std::cout << "[ThreadPool] Shutdown complete.\n";
}

void ThreadPool::Submit(std::function<void()> task)
{
    if (m_Stop) return; // guard against post-shutdown submit

    {
        std::scoped_lock lock(m_QueueMutex);
        m_Tasks.push(std::move(task));
    }
    m_Condition.notify_one();
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        std::function<void()> job;

        {
            std::unique_lock lock(m_QueueMutex);
            m_Condition.wait(lock, [this] { return m_Stop || !m_Tasks.empty(); });

            if (m_Stop && m_Tasks.empty())
                return;

            job = std::move(m_Tasks.front());
            m_Tasks.pop();
        }

        try
        {
            job();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[ThreadPool] Task threw exception: " << e.what() << "\n";
        }
    }
}

size_t ThreadPool::GetQueueSize() const
{
    std::scoped_lock lock(m_QueueMutex);
    return m_Tasks.size();
}
