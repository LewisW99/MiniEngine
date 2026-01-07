// MathTypes.h
#pragma once
#include <array>
#include <cmath>
#include <cfloat>

struct Vec3 {
    float x, y, z;

    Vec3(float X = 0, float Y = 0, float Z = 0) :x(X), y(Y), z(Z) {}
};

struct Mat4 {
    float m[16];
    static Mat4 Identity();
    static Mat4 FromTRS(const Vec3& pos, const Vec3& rot, const Vec3& scl);
    Mat4 operator*(const Mat4& rhs) const;
};

// Basic AABB (Axis-Aligned Bounding Box)
struct AABB {
    Vec3 min{ 0,0,0 };
    Vec3 max{ 0,0,0 };
    AABB Transform(const Mat4& m) const {
        // Compute all 8 corners of the box
        Vec3 corners[8] = {
            {min.x, min.y, min.z},
            {max.x, min.y, min.z},
            {min.x, max.y, min.z},
            {max.x, max.y, min.z},
            {min.x, min.y, max.z},
            {max.x, min.y, max.z},
            {min.x, max.y, max.z},
            {max.x, max.y, max.z},
        };

        AABB result;
        result.min = { FLT_MAX, FLT_MAX, FLT_MAX };
        result.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (auto& c : corners) {
            // Transform corner (assuming column-major Mat4)
            Vec3 t{
                m.m[0] * c.x + m.m[4] * c.y + m.m[8] * c.z + m.m[12],
                m.m[1] * c.x + m.m[5] * c.y + m.m[9] * c.z + m.m[13],
                m.m[2] * c.x + m.m[6] * c.y + m.m[10] * c.z + m.m[14]
            };
            result.min.x = std::min(result.min.x, t.x);
            result.min.y = std::min(result.min.y, t.y);
            result.min.z = std::min(result.min.z, t.z);
            result.max.x = std::max(result.max.x, t.x);
            result.max.y = std::max(result.max.y, t.y);
            result.max.z = std::max(result.max.z, t.z);
        }
        return result;
    }

};

inline Mat4 Mat4::Identity() {
    Mat4 r{};
    r.m[0] = 1; r.m[5] = 1; r.m[10] = 1; r.m[15] = 1;
    return r;
}

inline Mat4 Mat4::FromTRS(const Vec3& pos, const Vec3& rot, const Vec3& scl) {
    const float cx = std::cos(rot.x), sx = std::sin(rot.x);
    const float cy = std::cos(rot.y), sy = std::sin(rot.y);
    const float cz = std::cos(rot.z), sz = std::sin(rot.z);

    // Rotation matrices (XYZ order)
    Mat4 rx = Identity();
    rx.m[5] = cx;  rx.m[6] = -sx;
    rx.m[9] = sx;  rx.m[10] = cx;

    Mat4 ry = Identity();
    ry.m[0] = cy;  ry.m[2] = sy;
    ry.m[8] = -sy; ry.m[10] = cy;

    Mat4 rz = Identity();
    rz.m[0] = cz;  rz.m[1] = -sz;
    rz.m[4] = sz;  rz.m[5] = cz;

    // Combine R = Rz * Ry * Rx
    Mat4 rotMat = rz * ry * rx;

    // Apply scale
    Mat4 scaleMat = Identity();
    scaleMat.m[0] = scl.x;
    scaleMat.m[5] = scl.y;
    scaleMat.m[10] = scl.z;

    Mat4 trs = rotMat * scaleMat;

    // Apply translation
    trs.m[12] = pos.x;
    trs.m[13] = pos.y;
    trs.m[14] = pos.z;
    trs.m[15] = 1.0f;

    return trs;
}

inline Mat4 Mat4::operator*(const Mat4& rhs) const {
    Mat4 r = Identity();
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[col + row * 4] =
                m[row * 4 + 0] * rhs.m[col + 0] +
                m[row * 4 + 1] * rhs.m[col + 4] +
                m[row * 4 + 2] * rhs.m[col + 8] +
                m[row * 4 + 3] * rhs.m[col + 12];
        }
    }
    return r;
}
