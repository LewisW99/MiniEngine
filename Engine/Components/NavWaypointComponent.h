#pragma once

#include <vector>
#include "../ECS/Entity.h"

struct NavWaypointComponent
{
    std::vector<EntityID> links;
};
