#pragma once
#include <glm/vec3.hpp>

struct PhysicsComponent
{
    bool enabled = true;
    float mass = 1.0f;
    glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

    bool grounded = false;

    float jumpImpulse = 5.5f;
};
