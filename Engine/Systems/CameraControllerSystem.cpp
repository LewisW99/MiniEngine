#include "pch.h"
#include "CameraControllerSystem.h"

#include "../Components/CameraFollowComponent.h"
#include "../TransformSystem.h"
#include "../Rendering/CameraMode.h"
#include "../EditorConsole.h"

#include <glm/glm.hpp>
#include <cmath>
#include "../Components/PlayerControllerComponent.h"

static glm::vec3 Lerp(
    const glm::vec3& a,
    const glm::vec3& b,
    float t)
{
    return a + (b - a) * t;
}

void CameraControllerSystem::Update(
    EntityManager& entities,
    ComponentManager& components,
    Camera& camera,
    float dt
)
{
    for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
    {
        Entity camEntity{ id };

        if (!entities.IsAlive(camEntity))
            continue;

        if (!components.HasComponent<CameraFollowComponent>(camEntity))
            continue;

        auto& follow =
            components.GetComponent<CameraFollowComponent>(camEntity);

        Entity target = follow.target;

        if (!entities.IsAlive(target))
            continue;

        if (!components.HasComponent<TransformComponent>(target))
            continue;

        if (!components.HasComponent<PlayerControllerComponent>(target))
            continue;

        auto& targetTransform =
            components.GetComponent<TransformComponent>(target);

        auto& pc =
            components.GetComponent<PlayerControllerComponent>(target);

        // ============================================================
        // FIRST PERSON
        // ============================================================
        if (pc.cameraMode == CameraMode::FirstPerson)
        {
            camera.position.x = targetTransform.position.x;
            camera.position.y = targetTransform.position.y + follow.height;
            camera.position.z = targetTransform.position.z;

            // forward already handled by PCS
        }
        // ============================================================
        // THIRD PERSON
        // ============================================================
        else
        {
            glm::vec3 targetPos(
                targetTransform.position.x,
                targetTransform.position.y + follow.height,
                targetTransform.position.z
            );

            float yawRad =
                glm::radians(targetTransform.rotation.y);

            glm::vec3 forward(
                std::sin(yawRad),
                0.0f,
                std::cos(yawRad)
            );

            glm::vec3 desiredPos =
                targetPos - forward * follow.distance;

            float t =
                1.0f - std::exp(-follow.smoothness * dt);

            camera.position =
                Lerp(camera.position, desiredPos, t);

            camera.LookAt(targetPos);
        }

        // only one active camera
        break;
    }
}

