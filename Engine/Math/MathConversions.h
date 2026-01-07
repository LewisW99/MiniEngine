#pragma once
#include <glm/glm.hpp>
#include "MathTypes.h"

inline glm::vec3 ToGlm(const Vec3& v) {
    return glm::vec3(v.x, v.y, v.z);
}