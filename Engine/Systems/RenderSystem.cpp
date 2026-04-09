#include "pch.h"
#include "RenderSystem.h"

#include "../Components/MaterialComponent.h"
#include "../Components/MeshComponent.h"
#include "../Renderer.h"

void RenderSystem::Render(const EntityManager& entities,
    const ComponentManager& components,
    Renderer& renderer)
{
    for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) ||
            !components.HasComponent<TransformComponent>(entity) ||
            !components.HasComponent<MeshComponent>(entity))
        {
            continue;
        }

        const auto& transform = components.GetComponent<TransformComponent>(entity);
        const auto& mesh = components.GetComponent<MeshComponent>(entity);
        const MaterialComponent* material = components.HasComponent<MaterialComponent>(entity)
            ? &components.GetComponent<MaterialComponent>(entity)
            : nullptr;

        renderer.DrawRenderable(transform, mesh, material);
    }
}
