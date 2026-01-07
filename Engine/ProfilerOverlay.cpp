#include "pch.h"
#include "ProfilerOverlay.h"

ProfilerOverlay::ProfilerOverlay(Allocator* allocator)
    : m_Allocator(allocator), m_FrameCount(0), m_AccumTime(0.0f) {
}

void ProfilerOverlay::update(float frameTimeMs) {
    m_FrameCount++;
    m_AccumTime += frameTimeMs;

    if (m_AccumTime >= 1000.0f) { // once per second
        float fps = (m_FrameCount * 1000.0f) / m_AccumTime;

        auto stats = m_Allocator->getStats();

        std::cout << "==== Profiler Overlay ====\n";
        std::cout << "FPS: " << fps
            << " | Frame Time: " << (m_AccumTime / m_FrameCount) << " ms\n";
        std::cout << "Allocator Stats: total=" << stats.totalAllocated
            << " bytes, peak=" << stats.peakUsage
            << " bytes, allocations=" << stats.allocationCount << "\n";
        std::cout << "==========================\n";

        // reset counters
        m_FrameCount = 0;
        m_AccumTime = 0.0f;
    }
}
