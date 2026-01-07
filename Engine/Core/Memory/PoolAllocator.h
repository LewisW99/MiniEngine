#pragma once

#include "Allocator.h"
#include <cstdlib>
#include <cstddef>
#include <stdexcept>

class PoolAllocator : public Allocator {
public:
	AllocatorStats m_Stats;
     explicit PoolAllocator(size_t blockSize, size_t numBlocks)
        : m_BlockSize(blockSize), m_NumBlocks(numBlocks)
    {
        m_MemoryBlock = std::malloc(blockSize * numBlocks);
        if (!m_MemoryBlock) throw std::bad_alloc();

        reset();
    }

    ~PoolAllocator() override {
        std::free(m_MemoryBlock);
    }

    void* allocate(size_t size) override {
        if (size > m_BlockSize || !m_FreeList) return nullptr;

        // Pop from free list
        void* ptr = m_FreeList;
        m_FreeList = static_cast<void**>(*m_FreeList);

        m_Stats.totalAllocated += m_BlockSize;
        m_Stats.allocationCount++;
        if (m_Stats.totalAllocated > m_Stats.peakUsage)
            m_Stats.peakUsage = m_Stats.totalAllocated;

        return ptr;
    }

    void deallocate(void* ptr) override {
        // Push back to free list
        *static_cast<void**>(ptr) = m_FreeList;
        m_FreeList = static_cast<void**>(ptr);

        if (m_Stats.totalAllocated >= m_BlockSize)
            m_Stats.totalAllocated -= m_BlockSize;
        else
            m_Stats.totalAllocated = 0;

    }

    void reset() override {
        // Rebuild free list
        char* p = static_cast<char*>(m_MemoryBlock);
        for (size_t i = 0; i < m_NumBlocks - 1; ++i) {
            *reinterpret_cast<void**>(p) = p + m_BlockSize;
            p += m_BlockSize;
        }
        *reinterpret_cast<void**>(p) = nullptr; // last block
        m_FreeList = static_cast<void**>(m_MemoryBlock);

        m_Stats = {};



    }

    AllocatorStats getStats() const override { return m_Stats; }

private:
    size_t m_BlockSize;
    size_t m_NumBlocks;
    void* m_MemoryBlock;  // base pointer for free()
    void** m_FreeList;    // linked free list
};
