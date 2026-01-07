#pragma once

#include "Allocator.h"
#include <cstdlib>
#include <cstdint>
#include <stdexcept>

class StackMarker {
public:
    explicit StackMarker(char* ptr) : m_Ptr(ptr) {}
    char* get() const { return m_Ptr; }
private:
    char* m_Ptr;
};

class StackAllocator : public Allocator {
public:
	AllocatorStats m_Stats;

    explicit StackAllocator(size_t size)
        : m_Start(static_cast<char*>(std::malloc(size))),
        m_Current(m_Start),
        m_End(m_Start + size) {
    }

    ~StackAllocator() override {
        std::free(m_Start);
    }

    void* allocate(size_t size) override {
        // Align to 4 bytes
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
        // No-op: must use freeToMarker or reset
    }

    void reset() override {
        m_Current = m_Start;
		m_Stats.totalAllocated = 0; // reset current usage
    }

    StackMarker getMarker() const { return StackMarker(m_Current); }

    void freeToMarker(StackMarker marker) {
        if (marker.get() >= m_Start && marker.get() <= m_End) {
            // Compute how much memory we're "freeing"
            size_t freed = static_cast<size_t>(m_Current - marker.get());
            m_Current = marker.get();

            if (freed <= m_Stats.totalAllocated)
                m_Stats.totalAllocated -= freed;
            else
                m_Stats.totalAllocated = 0;
        }
        else {
            throw std::invalid_argument("Marker out of range");
        }
    }

    AllocatorStats getStats() const override { return m_Stats; }

private:
    char* m_Start;
    char* m_Current;
    char* m_End;
};
