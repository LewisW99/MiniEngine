#pragma once
#include <cstdint>
#include "../Math/MathTypes.h"

enum class CollisionMode
{
    None = 0,
    Blocking,
    Trigger
};

struct ColliderComponent
{
    Vec3  halfExtents{ 0.5f, 0.5f, 0.5f };

    bool isStatic = false;   // walls, floors, level geometry
    CollisionMode mode = CollisionMode::Blocking;
    std::uint32_t layer = 1u;
    std::uint32_t mask = 0xFFFFFFFFu;
};
