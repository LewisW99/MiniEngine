#pragma once
#include "../ECS/Entity.h"
#include "../Rendering/CameraMode.h"

struct CameraFollowComponent
{
    Entity target;              // entity to follow
    float distance = 5.0f;      // third-person distance
    float height = 2.0f;        // vertical offset
    float smoothness = 10.0f;   // interpolation speed
};
