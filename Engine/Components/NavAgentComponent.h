#pragma once

#include <vector>
#include "../ECS/Entity.h"

enum class NavAgentMode
{
    Patrol = 0,
    Follow,
    Chase
};

struct NavAgentComponent
{
    NavAgentMode mode = NavAgentMode::Patrol;
    std::vector<EntityID> waypointEntities;
    Entity targetEntity{};
    float speed = 2.0f;
    float stoppingDistance = 0.1f;
    uint32_t currentWaypointIndex = 0;
    bool loop = true;
    bool active = true;
};
