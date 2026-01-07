#pragma once


struct AllocatorStats {
    size_t totalAllocated = 0;   // current allocated bytes
    size_t peakUsage = 0;        // maximum allocated at once
    size_t allocationCount = 0;  // number of allocations ever made
};


class Allocator {
public:
    virtual ~Allocator() = default;
    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual void reset() = 0;
	virtual AllocatorStats getStats() const = 0;
};