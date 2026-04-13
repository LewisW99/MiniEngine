#pragma once
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include "../../ECS/Entity.h"

struct PhysicsComponent
{
    bool enabled = true;
    float mass = 1.0f;
    glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };

    bool grounded = false;

    float jumpImpulse = 5.5f;
    std::vector<EntityID> overlappingEntities{};
    std::vector<std::string> overlappingTags{};
    std::vector<EntityID> triggerEntities{};
    std::vector<std::string> triggerTags{};
    std::vector<EntityID> previousTriggerEntities{};
};
