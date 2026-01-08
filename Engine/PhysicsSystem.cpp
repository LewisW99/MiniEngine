#include "pch.h"
#include "PhysicsSystem.h"
#include "../Engine/TransformSystem.h"
#include "../Engine/Components/Physics/PhysicsComponent.h"

static constexpr float Gravity = -9.81f;

void PhysicsSystem::Update(EntityManager& entities, ComponentManager& comps, float dt)
{
    for (uint32_t id = 0; id < 10000; ++id)
    {
        Entity e{ id };
        if (!entities.IsAlive(e))
            continue;

        if (!comps.HasComponent<TransformComponent>(e) ||
            !comps.HasComponent<PhysicsComponent>(e))
            continue;

        auto& transform = comps.GetComponent<TransformComponent>(e);
        auto& physics = comps.GetComponent<PhysicsComponent>(e);

        if (!physics.enabled)
            continue;

        physics.velocity.y += Gravity * dt;
        transform.position.y += physics.velocity.y * dt;
    }
}
