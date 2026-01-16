#pragma once

#include "../ECS/EntityManager.h"
#include "../ECS/ComponentManager.h"
#include "../Rendering/Camera.h"

class CameraControllerSystem
{
public:
	static void Update(
		EntityManager& entities,
		ComponentManager& components,
		Camera& camera,
		float dt
	);
};