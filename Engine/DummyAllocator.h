#pragma once

#include "Core/Memory/Allocator.h"
#include <iostream>
#include <cstdlib>


class DummyAllocator : public Allocator
{
public:
	AllocatorStats m_Stats;
     void* allocate(size_t size) override
	{
		return std::malloc(size);
	}

	void deallocate(void* ptr) override
	{
		return std::free(ptr);
	}

	void reset()
	{
		//nothing rn
	}

	AllocatorStats getStats() const override
	{
		return m_Stats;
	}
};

