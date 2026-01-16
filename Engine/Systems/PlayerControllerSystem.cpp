#include "pch.h"
#include "PlayerControllerSystem.h"

#include "../Components/PlayerControllerComponent.h"
#include "../TransformSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

void PlayerControllerSystem::Update(
    EntityManager& entities,
    ComponentManager& components,
    InputSystem& input,
    Camera& camera,
    float dt
)
{
    for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
    {
        Entity e{ id };

        if (!entities.IsAlive(e))
            continue;

        if (!components.HasComponent<PlayerControllerComponent>(e))
            continue;

        if (!components.HasComponent<TransformComponent>(e))
            continue;

        auto& pc = components.GetComponent<PlayerControllerComponent>(e);
        auto& transform = components.GetComponent<TransformComponent>(e);

        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (input.Held("MoveForward"))  moveZ -= -1.0f;
        if (input.Held("MoveBackward")) moveZ += 1.0f;
        if (input.Held("MoveLeft"))     moveX -= 1.0f;
        if (input.Held("MoveRight"))    moveX += 1.0f;


        if (moveX != 0.0f || moveZ != 0.0f)
        {
            // Flatten camera forward to ground plane
            glm::vec3 camForward = camera.forward;
            camForward.y = 0.0f;

            if (glm::length(camForward) > 0.0f)
                camForward = glm::normalize(camForward);

            glm::vec3 camRight =
                glm::normalize(glm::cross(camForward, camera.up));

            glm::vec3 moveDir =
                camForward * moveZ +
                camRight * moveX;

            // Axis-style: normalize diagonal movement
            if (glm::length(moveDir) > 0.0f)
            {
                moveDir = glm::normalize(moveDir);

                glm::vec3 delta =
                    moveDir * pc.moveSpeed * dt;

                transform.position.x += delta.x;
                transform.position.y += delta.y;
                transform.position.z += delta.z;
            }
        }

        float mouseDX = -input.GetMouseDX();

        if (mouseDX != 0.0f)
        {
            transform.rotation.y += mouseDX * pc.lookSpeed;

            // Optional: keep rotation tidy
            if (transform.rotation.y > 360.0f)
                transform.rotation.y -= 360.0f;
            else if (transform.rotation.y < 0.0f)
                transform.rotation.y += 360.0f;
        }
    }
}
