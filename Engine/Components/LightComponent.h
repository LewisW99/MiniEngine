#pragma once

#include <glm/glm.hpp>

struct LightComponent
{
    glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
    glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    bool enabled = true;
};
