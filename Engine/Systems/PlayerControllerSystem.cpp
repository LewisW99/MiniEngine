#include "pch.h"
#include "PlayerControllerSystem.h"
#include "../Components/PlayerControllerComponent.h"
#include "../TransformSystem.h"
#include <cmath>

void PlayerControllerSystem::Update(
    EntityManager& entities,
    ComponentManager& components,
    InputSystem& input,
    float dt
)
{
    if (!input.IsGameplayEnabled())
        return;

    for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
    {
        Entity e{ id };

        if (!entities.IsAlive(e))
            continue;

        if (!components.HasComponent<PlayerControllerComponent>(e))
            continue;

        if (!components.HasComponent<TransformComponent>(e))
            continue;

        auto& controller =
            components.GetComponent<PlayerControllerComponent>(e);
        auto& transform =
            components.GetComponent<TransformComponent>(e);

        // ---------------- Movement ----------------
        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (input.Held("MoveForward"))  moveZ -= 1.0f;
        if (input.Held("MoveBackward")) moveZ += 1.0f;
        if (input.Held("MoveLeft"))     moveX -= 1.0f;
        if (input.Held("MoveRight"))    moveX += 1.0f;

        // Normalize so diagonals aren’t faster
        float length = std::sqrt(moveX * moveX + moveZ * moveZ);
        if (length > 0.0f)
        {
            moveX /= length;
            moveZ /= length;
        }

        transform.position.x += moveX * controller.moveSpeed * dt;
        transform.position.z += moveZ * controller.moveSpeed * dt;

        // ---------------- Look (Yaw only for now) ----------------
        float mouseDX = input.GetMouseDX();
        transform.rotation.y += mouseDX * controller.lookSpeed;
    }
}
