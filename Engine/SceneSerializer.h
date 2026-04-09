#pragma once

#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "Components/CameraFollowComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/LightComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/Physics/PhysicsComponent.h"
#include "Components/PlayerControllerComponent.h"
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "ECS/EntityMeta.h"
#include "Scripting/ScriptComponent.h"

using json = nlohmann::json;

class SceneSerializer
{
public:
    static void Save(const std::string& path,
        const EntityManager& entities,
        const ComponentManager& comps,
        const EntityMeta& meta)
    {
        json root;
        root["entities"] = json::array();

        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity))
            {
                continue;
            }

            json entry;
            entry["id"] = entity.id;
            entry["name"] = meta.names.count(entity.id)
                ? meta.names.at(entity.id)
                : "Entity " + std::to_string(entity.id);

            if (comps.HasComponent<TransformComponent>(entity))
            {
                const auto& transform = comps.GetComponent<TransformComponent>(entity);
                entry["transform"] = {
                    { "position", { transform.position.x, transform.position.y, transform.position.z } },
                    { "rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
                    { "scale", { transform.scale.x, transform.scale.y, transform.scale.z } }
                };
            }

            if (comps.HasComponent<MeshComponent>(entity))
            {
                const auto& mesh = comps.GetComponent<MeshComponent>(entity);
                entry["mesh"] = {
                    { "path", mesh.meshPath }
                };
            }

            if (comps.HasComponent<MaterialComponent>(entity))
            {
                const auto& material = comps.GetComponent<MaterialComponent>(entity);
                entry["material"] = {
                    { "albedo", { material.albedo.x, material.albedo.y, material.albedo.z } },
                    { "specular", material.specular },
                    { "shininess", material.shininess },
                    { "albedoTexture", material.albedoTexture },
                    { "useTexture", material.useTexture }
                };
            }

            if (comps.HasComponent<LightComponent>(entity))
            {
                const auto& light = comps.GetComponent<LightComponent>(entity);
                entry["light"] = {
                    { "direction", { light.direction.x, light.direction.y, light.direction.z } },
                    { "color", { light.color.x, light.color.y, light.color.z } },
                    { "intensity", light.intensity },
                    { "enabled", light.enabled }
                };
            }

            if (comps.HasComponent<PhysicsComponent>(entity))
            {
                const auto& physics = comps.GetComponent<PhysicsComponent>(entity);
                entry["physics"] = {
                    { "enabled", physics.enabled },
                    { "mass", physics.mass }
                };
            }

            if (comps.HasComponent<ColliderComponent>(entity))
            {
                const auto& collider = comps.GetComponent<ColliderComponent>(entity);
                entry["collider"] = {
                    { "halfExtents", { collider.halfExtents.x, collider.halfExtents.y, collider.halfExtents.z } },
                    { "isStatic", collider.isStatic }
                };
            }

            if (comps.HasComponent<ScriptComponent>(entity))
            {
                const auto& script = comps.GetComponent<ScriptComponent>(entity);
                entry["script"] = {
                    { "path", script.ScriptPath }
                };
            }

            if (comps.HasComponent<PlayerControllerComponent>(entity))
            {
                const auto& controller = comps.GetComponent<PlayerControllerComponent>(entity);
                entry["playerController"] = {
                    { "moveSpeed", controller.moveSpeed },
                    { "lookSpeed", controller.lookSpeed }
                };
            }

            if (comps.HasComponent<CameraFollowComponent>(entity))
            {
                const auto& follow = comps.GetComponent<CameraFollowComponent>(entity);
                entry["cameraFollow"] = {
                    { "target", follow.target.id },
                    { "distance", follow.distance },
                    { "height", follow.height },
                    { "smoothness", follow.smoothness }
                };
            }

            root["entities"].push_back(entry);
        }

        std::ofstream file(path);
        file << root.dump(4);
    }

    static void Load(const std::string& path,
        EntityManager& entities,
        ComponentManager& comps,
        EntityMeta& meta)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return;
        }

        json root;
        file >> root;

        entities.Clear();
        comps.Clear();
        meta.Clear();

        for (const auto& entry : root["entities"])
        {
            const Entity entity = entities.CreateEntity();
            meta.SetName(entity, entry.value("name", "Entity " + std::to_string(entity.id)));

            const bool hasTransform = entry.contains("transform");
            if (hasTransform)
            {
                TransformComponent transform;
                const auto& tr = entry["transform"];

                transform.position = { tr["position"][0], tr["position"][1], tr["position"][2] };
                transform.rotation = { tr["rotation"][0], tr["rotation"][1], tr["rotation"][2] };
                transform.scale = { tr["scale"][0], tr["scale"][1], tr["scale"][2] };

                comps.AddComponent(entity, transform);
            }

            if (entry.contains("mesh"))
            {
                MeshComponent mesh;
                mesh.meshPath = entry["mesh"].value("path", std::string{ "builtin://cube" });
                comps.AddComponent(entity, mesh);
            }
            else if (hasTransform)
            {
                comps.AddComponent(entity, MeshComponent{});
            }

            if (entry.contains("material"))
            {
                MaterialComponent material;
                const auto& materialEntry = entry["material"];

                if (materialEntry.contains("albedo") && materialEntry["albedo"].size() >= 3)
                {
                    material.albedo = {
                        materialEntry["albedo"][0],
                        materialEntry["albedo"][1],
                        materialEntry["albedo"][2]
                    };
                }

                material.specular = materialEntry.value("specular", material.specular);
                material.shininess = materialEntry.value("shininess", material.shininess);
                material.albedoTexture = materialEntry.value(
                    "albedoTexture",
                    materialEntry.value("albedoTexturePath", std::string{}));
                material.useTexture = materialEntry.value("useTexture", false);
                comps.AddComponent(entity, material);
            }

            if (entry.contains("light"))
            {
                LightComponent light;
                const auto& lightEntry = entry["light"];

                if (lightEntry.contains("direction") && lightEntry["direction"].size() >= 3)
                {
                    light.direction = {
                        lightEntry["direction"][0],
                        lightEntry["direction"][1],
                        lightEntry["direction"][2]
                    };
                }

                if (lightEntry.contains("color") && lightEntry["color"].size() >= 3)
                {
                    light.color = {
                        lightEntry["color"][0],
                        lightEntry["color"][1],
                        lightEntry["color"][2]
                    };
                }

                light.intensity = lightEntry.value("intensity", light.intensity);
                light.enabled = lightEntry.value("enabled", true);
                comps.AddComponent(entity, light);
            }

            if (entry.contains("physics"))
            {
                PhysicsComponent physics;
                physics.enabled = entry["physics"].value("enabled", true);
                physics.mass = entry["physics"].value("mass", 1.0f);
                comps.AddComponent(entity, physics);
            }

            if (entry.contains("collider"))
            {
                ColliderComponent collider;
                const auto& colliderEntry = entry["collider"];
                collider.halfExtents = {
                    colliderEntry["halfExtents"][0],
                    colliderEntry["halfExtents"][1],
                    colliderEntry["halfExtents"][2]
                };
                collider.isStatic = colliderEntry.value("isStatic", false);
                comps.AddComponent(entity, collider);
            }

            if (entry.contains("script"))
            {
                ScriptComponent script;
                script.ScriptPath = entry["script"].value("path", "");
                comps.AddComponent(entity, script);
            }

            if (entry.contains("playerController"))
            {
                PlayerControllerComponent controller;
                controller.moveSpeed = entry["playerController"].value("moveSpeed", 5.0f);
                controller.lookSpeed = entry["playerController"].value("lookSpeed", 0.1f);
                comps.AddComponent(entity, controller);
            }

            if (entry.contains("cameraFollow"))
            {
                CameraFollowComponent follow;
                follow.target = Entity{ entry["cameraFollow"].value("target", uint32_t{ 0 }) };
                follow.distance = entry["cameraFollow"].value("distance", 5.0f);
                follow.height = entry["cameraFollow"].value("height", 2.0f);
                follow.smoothness = entry["cameraFollow"].value("smoothness", 10.0f);
                comps.AddComponent(entity, follow);
            }
        }
    }
};
