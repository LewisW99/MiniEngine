#pragma once

#include "../ECS/EntityManager.h"
#include "../ECS/ComponentManager.h"
#include "../InputSystem.h"
#include "../Rendering/Camera.h"

class PlayerControllerSystem
{
public:
    static void Update(
        EntityManager& entities,
        ComponentManager& components,
        InputSystem& input,
        Camera& camera,
        float dt
    );

};
