#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Components/AnimationComponent.h"
#include "Components/LightComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/NavAgentComponent.h"
#include "Components/Physics/PhysicsComponent.h"
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "ECS/EntityMeta.h"
#include "TransformSystem.h"

using savegame_json = nlohmann::json;

class SaveGameManager
{
public:
    static bool Save(const std::filesystem::path& path, const EntityManager& entities, const ComponentManager& components, const EntityMeta& meta)
    {
        savegame_json root;
        root["entities"] = savegame_json::array();

        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity) || !meta.names.count(entity.id))
            {
                continue;
            }

            savegame_json entry;
            entry["name"] = meta.names.at(entity.id);

            if (components.HasComponent<TransformComponent>(entity))
            {
                const auto& transform = components.GetComponent<TransformComponent>(entity);
                entry["transform"] = {
                    { "position", { transform.position.x, transform.position.y, transform.position.z } },
                    { "rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
                    { "scale", { transform.scale.x, transform.scale.y, transform.scale.z } }
                };
            }

            if (components.HasComponent<PhysicsComponent>(entity))
            {
                const auto& physics = components.GetComponent<PhysicsComponent>(entity);
                entry["physics"] = {
                    { "enabled", physics.enabled },
                    { "velocity", { physics.velocity.x, physics.velocity.y, physics.velocity.z } }
                };
            }

            if (components.HasComponent<MaterialComponent>(entity))
            {
                const auto& material = components.GetComponent<MaterialComponent>(entity);
                entry["material"] = {
                    { "albedo", { material.albedo.x, material.albedo.y, material.albedo.z } },
                    { "specular", material.specular },
                    { "shininess", material.shininess },
                    { "albedoTexture", material.albedoTexture },
                    { "useTexture", material.useTexture }
                };
            }

            if (components.HasComponent<LightComponent>(entity))
            {
                const auto& light = components.GetComponent<LightComponent>(entity);
                entry["light"] = {
                    { "color", { light.color.x, light.color.y, light.color.z } },
                    { "intensity", light.intensity },
                    { "enabled", light.enabled }
                };
            }

            if (components.HasComponent<AnimationComponent>(entity))
            {
                const auto& animation = components.GetComponent<AnimationComponent>(entity);
                entry["animation"] = {
                    { "playing", animation.playing },
                    { "loop", animation.loop },
                    { "currentTime", animation.currentTime }
                };
            }

            if (components.HasComponent<NavAgentComponent>(entity))
            {
                const auto& nav = components.GetComponent<NavAgentComponent>(entity);
                entry["navigation"] = {
                    { "active", nav.active },
                    { "waypointIndex", nav.currentWaypointIndex }
                };
            }

            root["entities"].push_back(entry);
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        file << root.dump(4);
        return true;
    }

    static bool Load(const std::filesystem::path& path, const EntityManager& entities, ComponentManager& components, const EntityMeta& meta)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        savegame_json root;
        file >> root;

        for (const auto& entry : root["entities"])
        {
            const std::string name = entry.value("name", "");
            Entity matched{};
            for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
            {
                const Entity entity{ id };
                if (entities.IsAlive(entity) && meta.names.count(entity.id) && meta.names.at(entity.id) == name)
                {
                    matched = entity;
                    break;
                }
            }

            if (!matched)
            {
                continue;
            }

            if (entry.contains("transform") && components.HasComponent<TransformComponent>(matched))
            {
                auto& transform = components.GetComponent<TransformComponent>(matched);
                const auto& transformEntry = entry["transform"];
                transform.position = { transformEntry["position"][0], transformEntry["position"][1], transformEntry["position"][2] };
                transform.rotation = { transformEntry["rotation"][0], transformEntry["rotation"][1], transformEntry["rotation"][2] };
                transform.scale = { transformEntry["scale"][0], transformEntry["scale"][1], transformEntry["scale"][2] };
            }

            if (entry.contains("physics") && components.HasComponent<PhysicsComponent>(matched))
            {
                auto& physics = components.GetComponent<PhysicsComponent>(matched);
                const auto& physicsEntry = entry["physics"];
                physics.enabled = physicsEntry.value("enabled", physics.enabled);
                physics.velocity = {
                    physicsEntry["velocity"][0],
                    physicsEntry["velocity"][1],
                    physicsEntry["velocity"][2]
                };
            }

            if (entry.contains("material") && components.HasComponent<MaterialComponent>(matched))
            {
                auto& material = components.GetComponent<MaterialComponent>(matched);
                const auto& materialEntry = entry["material"];
                material.albedo = {
                    materialEntry["albedo"][0],
                    materialEntry["albedo"][1],
                    materialEntry["albedo"][2]
                };
                material.specular = materialEntry.value("specular", material.specular);
                material.shininess = materialEntry.value("shininess", material.shininess);
                material.albedoTexture = materialEntry.value("albedoTexture", material.albedoTexture);
                material.useTexture = materialEntry.value("useTexture", material.useTexture);
            }

            if (entry.contains("light") && components.HasComponent<LightComponent>(matched))
            {
                auto& light = components.GetComponent<LightComponent>(matched);
                const auto& lightEntry = entry["light"];
                light.color = { lightEntry["color"][0], lightEntry["color"][1], lightEntry["color"][2] };
                light.intensity = lightEntry.value("intensity", light.intensity);
                light.enabled = lightEntry.value("enabled", light.enabled);
            }

            if (entry.contains("animation") && components.HasComponent<AnimationComponent>(matched))
            {
                auto& animation = components.GetComponent<AnimationComponent>(matched);
                const auto& animationEntry = entry["animation"];
                animation.playing = animationEntry.value("playing", animation.playing);
                animation.loop = animationEntry.value("loop", animation.loop);
                animation.currentTime = animationEntry.value("currentTime", animation.currentTime);
            }

            if (entry.contains("navigation") && components.HasComponent<NavAgentComponent>(matched))
            {
                auto& nav = components.GetComponent<NavAgentComponent>(matched);
                const auto& navEntry = entry["navigation"];
                nav.active = navEntry.value("active", nav.active);
                nav.currentWaypointIndex = navEntry.value("waypointIndex", nav.currentWaypointIndex);
            }
        }

        return true;
    }
};
