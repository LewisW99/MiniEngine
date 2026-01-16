#include "pch.h"
#include "PlayerControllerSystem.h"

#include "../Components/PlayerControllerComponent.h"
#include "../Components/CameraFollowComponent.h"
#include "../Components/Physics/PhysicsComponent.h"
#include "../TransformSystem.h"
#include "../Rendering/CameraMode.h"

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

        if (!components.HasComponent<PhysicsComponent>(e))
            continue;

        auto& pc =
            components.GetComponent<PlayerControllerComponent>(e);

        auto& transform =
            components.GetComponent<TransformComponent>(e);

        auto& physics =
            components.GetComponent<PhysicsComponent>(e);

        // ============================================================
        // MOVEMENT INPUT → VELOCITY (X/Z ONLY)
        // ============================================================

        float moveX = input.GetAxis("MoveX");
        float moveZ = input.GetAxis("MoveZ");

        glm::vec3 desiredVelocity(0.0f);

        if (moveX != 0.0f || moveZ != 0.0f)
        {
            glm::vec3 camForward = camera.forward;
            camForward.y = 0.0f;

            if (glm::length(camForward) > 0.0f)
                camForward = glm::normalize(camForward);

            glm::vec3 camRight =
                glm::normalize(glm::cross(camForward, camera.up));

            glm::vec3 moveDir =
                camForward * moveZ +
                camRight * moveX;

            if (glm::length(moveDir) > 0.0f)
                moveDir = glm::normalize(moveDir);

            desiredVelocity =
                moveDir * pc.moveSpeed;
        }

        // Apply to physics (X/Z only)
        physics.velocity.x = desiredVelocity.x;
        physics.velocity.z = desiredVelocity.z;

        if (input.Pressed("Jump") && physics.grounded)
        {
            physics.velocity.y = physics.jumpImpulse;
            physics.grounded = false;
        }

        // ============================================================
        // CAMERA MODE SWITCH
        // ============================================================

        if (pc.allowCameraSwitch && input.Pressed("ToggleCamera"))
        {
            pc.cameraMode =
                (pc.cameraMode == CameraMode::ThirdPerson)
                ? CameraMode::FirstPerson
                : CameraMode::ThirdPerson;
        }

        // ============================================================
        // MOUSE LOOK / ROTATION
        // ============================================================

        float mouseDX = -input.GetMouseDX();
        float mouseDY = input.GetMouseDY();

        if (mouseDX != 0.0f || mouseDY != 0.0f)
        {
            // Rotate player yaw
            transform.rotation.y += mouseDX * pc.lookSpeed;

            if (pc.cameraMode == CameraMode::FirstPerson)
            {
                camera.yaw += mouseDX * camera.mouseSensitivity;
                camera.pitch -= mouseDY * camera.mouseSensitivity;
                camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);

                glm::vec3 dir;
                dir.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
                dir.y = sin(glm::radians(camera.pitch));
                dir.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
                camera.forward = glm::normalize(dir);
            }
        }
    }
}
