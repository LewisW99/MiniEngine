#pragma once
#include <functional>
#include <atomic>
#include "ThreadPool.h"
#include <vector>

struct Job {
    std::function<void()> task;   // the actual work
    std::atomic<int> remaining{ 1 }; // refcount/dependency counter
    Job* parent = nullptr;         // optional parent (for dependency trees)
};

class JobSystem {
public:
    explicit JobSystem(size_t threadCount = std::thread::hardware_concurrency());
    ~JobSystem();

    Job* CreateJob(std::function<void()> task, Job* parent = nullptr);

    void Run(Job* job);           // submits to thread pool
    void Wait(Job* job);          // blocks until finished

private:
    ThreadPool m_Pool;

    void Finish(Job* job);
};
