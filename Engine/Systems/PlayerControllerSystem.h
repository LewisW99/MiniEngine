#pragma once

#include "../ECS/EntityManager.h"
#include "../ECS/ComponentManager.h"
#include "../InputSystem.h"

class PlayerControllerSystem
{
public:
    static void Update(
        EntityManager& entities,
        ComponentManager& components,
        InputSystem& input,
        float dt
    );
};
