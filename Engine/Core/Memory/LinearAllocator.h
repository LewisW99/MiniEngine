#pragma once

#include "Allocator.h"
#include <cstdlib>
#include <cstdint>

class LinearAllocator : public Allocator {
public:

    AllocatorStats m_Stats;

    explicit LinearAllocator(size_t size)
        : m_Start(static_cast<char*>(std::malloc(size))),
        m_Current(m_Start),
        m_End(m_Start + size) {
    }




    ~LinearAllocator() {
        std::free(m_Start);
    }

    void* allocate(size_t size) override {
        // Align to 4 bytes (optional)
        uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_Current);
        uintptr_t alignedAddr = (currentAddr + 3) & ~static_cast<uintptr_t>(3);
        char* alignedPtr = reinterpret_cast<char*>(alignedAddr);

        if (alignedPtr + size > m_End) return nullptr;

        m_Current = alignedPtr + size;

        m_Stats.totalAllocated += size;
        m_Stats.allocationCount++;
        if (m_Stats.totalAllocated > m_Stats.peakUsage)
            m_Stats.peakUsage = m_Stats.totalAllocated;

        return alignedPtr;
    }

    void deallocate(void* /*ptr*/) override {
        // Linear allocator doesn’t support individual frees
    }

    void reset() override {
        m_Current = m_Start;
        m_Stats.totalAllocated = 0; // reset current usage
    }

    AllocatorStats getStats() const override {
        return m_Stats;
	}
 
private:
    char* m_Start;
    char* m_Current;
    char* m_End;
};
