#pragma once
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/ECS/ComponentManager.h"

namespace PhysicsSystem
{
    void Update(EntityManager& entities, ComponentManager& comps, float dt);
}
