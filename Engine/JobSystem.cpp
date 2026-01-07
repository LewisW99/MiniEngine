#include "pch.h"
#include "JobSystem.h"
#include <thread>

JobSystem::JobSystem(size_t threadCount)
    : m_Pool(threadCount - 1) {
}   // keep one core for main thread

JobSystem::~JobSystem() = default;

Job* JobSystem::CreateJob(std::function<void()> task, Job* parent)
{
    Job* job = new Job();
    job->task = std::move(task);
    job->parent = parent;
    if (parent) parent->remaining.fetch_add(1);
    return job;
}

void JobSystem::Run(Job* job)
{
    m_Pool.Submit([this, job]() {
        job->task();
        Finish(job);
        });
}

void JobSystem::Finish(Job* job)
{
    // Decrement this job’s counter; if still > 0, not done yet
    if (--job->remaining > 0)
        return;

    // If this job has a parent, mark the parent as one step closer to completion
    if (job->parent)
        Finish(job->parent);

    // Delete the job now that it’s done
    delete job;
}

void JobSystem::Wait(Job* job)
{
    while (job->remaining > 0)
        std::this_thread::yield(); // light spin-wait
}
