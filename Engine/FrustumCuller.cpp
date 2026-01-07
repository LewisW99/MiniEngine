#include "pch.h"
#include "FrustumCuller.h"
#include <cmath>

// ------------------------------------------------------------
// Extract planes from a view-projection matrix
// ------------------------------------------------------------
Frustum Frustum::FromMatrix(const Mat4& m) {
    Frustum f;

    // Reminder: our Mat4 uses column-major order (OpenGL-style)
    const float* a = m.m;

    // Left
    f.planes[LEFT].normal = { a[3] + a[0], a[7] + a[4], a[11] + a[8] };
    f.planes[LEFT].d = a[15] + a[12];

    // Right
    f.planes[RIGHT].normal = { a[3] - a[0], a[7] - a[4], a[11] - a[8] };
    f.planes[RIGHT].d = a[15] - a[12];

    // Bottom
    f.planes[BOTTOM].normal = { a[3] + a[1], a[7] + a[5], a[11] + a[9] };
    f.planes[BOTTOM].d = a[15] + a[13];

    // Top
    f.planes[TOP].normal = { a[3] - a[1], a[7] - a[5], a[11] - a[9] };
    f.planes[TOP].d = a[15] - a[13];

    // Near
    f.planes[NEAR].normal = { a[3] + a[2], a[7] + a[6], a[11] + a[10] };
    f.planes[NEAR].d = a[15] + a[14];

    // Far
    f.planes[FAR].normal = { a[3] - a[2], a[7] - a[6], a[11] - a[10] };
    f.planes[FAR].d = a[15] - a[14];

    // Normalize all planes
    for (auto& p : f.planes) {
        float len = std::sqrt(p.normal.x * p.normal.x + p.normal.y * p.normal.y + p.normal.z * p.normal.z);
        p.normal.x /= len; p.normal.y /= len; p.normal.z /= len;
        p.d /= len;
    }
    return f;
}

// ------------------------------------------------------------
// Test if AABB is inside or intersects the frustum
// ------------------------------------------------------------
bool FrustumCuller::IsVisible(const AABB& box, const Frustum& frustum) {
    for (const auto& plane : frustum.planes) {
        // Compute the most positive vertex relative to plane normal
        Vec3 positive{
            (plane.normal.x >= 0) ? box.max.x : box.min.x,
            (plane.normal.y >= 0) ? box.max.y : box.min.y,
            (plane.normal.z >= 0) ? box.max.z : box.min.z
        };

        // If that vertex is outside the plane, box is fully outside
        if (plane.Distance(positive) < 0)
            return false;
    }
    return true;
}

