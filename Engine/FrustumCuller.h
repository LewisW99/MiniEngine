#pragma once
#include "../Engine/Math/MathTypes.h"
#include <array>

// ------------------------------------------------------------
// Plane structure (normal + distance)
// ------------------------------------------------------------
struct Plane {
    Vec3 normal;
    float d;
    float Distance(const Vec3& p) const { return normal.x * p.x + normal.y * p.y + normal.z * p.z + d; }
};

// ------------------------------------------------------------
// Frustum - 6 planes extracted from view-projection matrix
// ------------------------------------------------------------
struct Frustum {
    enum { LEFT, RIGHT, TOP, BOTTOM, NEAR, FAR };
    std::array<Plane, 6> planes;

    static Frustum FromMatrix(const Mat4& viewProj);
};

// ------------------------------------------------------------
// FrustumCuller
// ------------------------------------------------------------
class FrustumCuller {
public:
    static bool IsVisible(const AABB& box, const Frustum& frustum);
};
