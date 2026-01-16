#pragma once
#include "../Engine/Math/MathTypes.h"

struct ColliderComponent
{
    Vec3  halfExtents{ 0.5f, 0.5f, 0.5f };

    bool isStatic = false;   // walls, floors, level geometry
};
