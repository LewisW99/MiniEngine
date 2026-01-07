#pragma once
#include "pch.h"
#include <iostream>
#include <chrono>
#include "../Engine/Core/Memory/Allocator.h"

class ProfilerOverlay {
public:
    explicit ProfilerOverlay(Allocator* allocator);

    // Called every frame with frame time (ms)
    void update(float frameTimeMs);

private:
    Allocator* m_Allocator;
    int m_FrameCount;
    float m_AccumTime; // accumulate until 1s has passed
};
