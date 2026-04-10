#pragma once

#include <cmath>
#include "../Components/NavAgentComponent.h"
#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"
#include "../TransformSystem.h"

class NavigationSystem
{
public:
    static void Update(const EntityManager& entities, ComponentManager& components, const float dt)
    {
        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity) ||
                !components.HasComponent<NavAgentComponent>(entity) ||
                !components.HasComponent<TransformComponent>(entity))
            {
                continue;
            }

            auto& agent = components.GetComponent<NavAgentComponent>(entity);
            if (!agent.active)
            {
                continue;
            }

            Vec3* target = ResolveTarget(agent, entities, components);
            if (target == nullptr)
            {
                continue;
            }

            auto& transform = components.GetComponent<TransformComponent>(entity);
            const Vec3 delta{ target->x - transform.position.x, target->y - transform.position.y, target->z - transform.position.z };
            const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            if (distance <= agent.stoppingDistance)
            {
                AdvanceWaypoint(agent);
                continue;
            }

            const float invDistance = distance > 0.0f ? 1.0f / distance : 0.0f;
            const float moveDistance = std::min(agent.speed * dt, distance);
            transform.position.x += delta.x * invDistance * moveDistance;
            transform.position.y += delta.y * invDistance * moveDistance;
            transform.position.z += delta.z * invDistance * moveDistance;
        }
    }

private:
    static Vec3* ResolveTarget(NavAgentComponent& agent, const EntityManager& entities, ComponentManager& components)
    {
        if ((agent.mode == NavAgentMode::Follow || agent.mode == NavAgentMode::Chase) &&
            agent.targetEntity &&
            entities.IsAlive(agent.targetEntity) &&
            components.HasComponent<TransformComponent>(agent.targetEntity))
        {
            return &components.GetComponent<TransformComponent>(agent.targetEntity).position;
        }

        if (agent.waypointEntities.empty())
        {
            return nullptr;
        }

        if (agent.currentWaypointIndex >= agent.waypointEntities.size())
        {
            agent.currentWaypointIndex = 0;
        }

        const Entity waypoint{ agent.waypointEntities[agent.currentWaypointIndex] };
        if (!entities.IsAlive(waypoint) || !components.HasComponent<TransformComponent>(waypoint))
        {
            return nullptr;
        }

        return &components.GetComponent<TransformComponent>(waypoint).position;
    }

    static void AdvanceWaypoint(NavAgentComponent& agent)
    {
        if (agent.waypointEntities.empty())
        {
            return;
        }

        ++agent.currentWaypointIndex;
        if (agent.currentWaypointIndex >= agent.waypointEntities.size())
        {
            agent.currentWaypointIndex = agent.loop ? 0U : static_cast<uint32_t>(agent.waypointEntities.size() - 1);
        }
    }
};
