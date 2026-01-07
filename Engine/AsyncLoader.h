#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <future>
#include <iostream>
#include "../Engine/JobSystem.h"

// ------------------------------------------------------------
// AsyncLoader - Simulates async file loading via JobSystem
// ------------------------------------------------------------
class AsyncLoader {
public:
    explicit AsyncLoader(JobSystem& js) : m_JobSystem(js) {}

    void RequestLoad(const std::string& assetName) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_LoadQueue.push(assetName);
    }

    void Update() {
        std::queue<std::string> pending;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::swap(pending, m_LoadQueue);
        }

        while (!pending.empty()) {
            std::string asset = pending.front();
            pending.pop();

            Job* job = m_JobSystem.CreateJob([asset]() {
                // Simulate file I/O latency
                std::this_thread::sleep_for(std::chrono::milliseconds(100 + rand() % 200));
                std::cout << "[AsyncLoader] Loaded asset: " << asset
                    << " on thread " << std::this_thread::get_id() << "\n";
                });
            m_JobSystem.Run(job);
        }
    }

private:
    JobSystem& m_JobSystem;
    std::mutex m_Mutex;
    std::queue<std::string> m_LoadQueue;
};
