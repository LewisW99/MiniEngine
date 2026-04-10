#pragma once

#include <algorithm>
#include "../Components/AnimationComponent.h"
#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"
#include "../TransformSystem.h"

class AnimationSystem
{
public:
    static void Update(const EntityManager& entities, ComponentManager& components, const float dt)
    {
        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity) ||
                !components.HasComponent<AnimationComponent>(entity) ||
                !components.HasComponent<TransformComponent>(entity))
            {
                continue;
            }

            auto& animation = components.GetComponent<AnimationComponent>(entity);
            if (!animation.playing || animation.clip.duration <= 0.0f)
            {
                continue;
            }

            animation.currentTime += dt;
            if (animation.loop)
            {
                while (animation.currentTime > animation.clip.duration)
                {
                    animation.currentTime -= animation.clip.duration;
                }
            }
            else if (animation.currentTime > animation.clip.duration)
            {
                animation.currentTime = animation.clip.duration;
                animation.playing = false;
            }

            auto& transform = components.GetComponent<TransformComponent>(entity);
            transform.position = Sample(animation.clip.positionKeys, animation.currentTime, transform.position);
            transform.rotation = Sample(animation.clip.rotationKeys, animation.currentTime, transform.rotation);
            transform.scale = Sample(animation.clip.scaleKeys, animation.currentTime, transform.scale);
        }
    }

private:
    static Vec3 Lerp(const Vec3& a, const Vec3& b, const float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    static Vec3 Sample(const std::vector<TransformKeyframe>& keys, const float time, const Vec3& fallback)
    {
        if (keys.empty())
        {
            return fallback;
        }

        if (keys.size() == 1 || time <= keys.front().time)
        {
            return keys.front().value;
        }

        for (size_t index = 1; index < keys.size(); ++index)
        {
            if (time <= keys[index].time)
            {
                const auto& previous = keys[index - 1];
                const auto& current = keys[index];
                const float segmentDuration = current.time - previous.time;
                const float alpha = segmentDuration > 0.0f ? (time - previous.time) / segmentDuration : 0.0f;
                return Lerp(previous.value, current.value, std::clamp(alpha, 0.0f, 1.0f));
            }
        }

        return keys.back().value;
    }
};
