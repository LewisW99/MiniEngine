#pragma once

#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"

class Renderer;

class RenderSystem
{
public:
    static void Render(const EntityManager& entities,
        const ComponentManager& components,
        Renderer& renderer);
};
