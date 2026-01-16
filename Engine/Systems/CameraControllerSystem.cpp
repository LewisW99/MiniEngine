#include "pch.h"
#include "CameraControllerSystem.h"
#include "../Components/CameraFollowComponent.h"
#include "../TransformSystem.h"
#include <glm/glm.hpp>
#include "../EditorConsole.h"

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
        Entity e{ id };

        if (!entities.IsAlive(e))
            continue;

        if (!components.HasComponent<CameraFollowComponent>(e))
            continue;

        const auto& follow =
            components.GetComponent<CameraFollowComponent>(e);

        if (!entities.IsAlive(follow.target))
            continue;

        if (!components.HasComponent<TransformComponent>(follow.target))
            continue;

        const auto& targetTransform =
            components.GetComponent<TransformComponent>(follow.target);

        

        // Target position (entity + height)
        glm::vec3 targetPos(
            targetTransform.position.x,
            targetTransform.position.y + follow.height,
            targetTransform.position.z
        );

        EditorConsole::Log(
            "Camera follow target: " +
            std::to_string(follow.target.id)
        );

        // Forward direction from target rotation (Y only for now)
        float yaw = glm::radians(targetTransform.rotation.y);
        glm::vec3 forward(
            sin(yaw),
            0.0f,
            cos(yaw)
        );

        glm::vec3 desiredPos =
            targetPos - forward * follow.distance;

        // Smooth follow
        glm::vec3 currentPos(
            camera.position.x,
            camera.position.y,
            camera.position.z
        );

        float t = 1.0f - expf(-follow.smoothness * dt);
        glm::vec3 newPos = Lerp(currentPos, desiredPos, t);

        camera.position.x = newPos.x;
        camera.position.y = newPos.y;
        camera.position.z = newPos.z;

        camera.LookAt(targetPos);

        // Only one active camera follow allowed
        break;
    }
}