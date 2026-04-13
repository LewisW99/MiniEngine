#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Components/AudioSourceComponent.h"
#include "Components/CameraFollowComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/DialogueComponent.h"
#include "Components/LightComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/Physics/PhysicsComponent.h"
#include "Components/PlayerControllerComponent.h"
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "ECS/EntityMeta.h"
#include "Scripting/ScriptComponent.h"
#include "TransformSystem.h"

using prefab_json = nlohmann::json;

class PrefabSerializer
{
public:
    static bool Save(const std::filesystem::path& path,
        const Entity entity,
        const ComponentManager& comps,
        const EntityMeta& meta)
    {
        if (!entity || !comps.HasComponent<TransformComponent>(entity))
        {
            return false;
        }

        prefab_json root;
        root["name"] = meta.names.count(entity.id) ? meta.names.at(entity.id) : "Prefab";
        WriteEntity(root["entity"], entity, comps);

        std::ofstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        file << root.dump(4);
        return true;
    }

    static Entity Instantiate(const std::filesystem::path& path,
        EntityManager& entities,
        ComponentManager& comps,
        EntityMeta& meta,
        const Vec3* spawnPosition = nullptr)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return {};
        }

        prefab_json root;
        file >> root;
        if (!root.contains("entity"))
        {
            return {};
        }

        const Entity entity = entities.CreateEntity();
        const auto& entry = root["entity"];
        const std::string defaultName = root.value("name", "Prefab Entity");
        meta.SetName(entity, defaultName);
        ReadEntity(entry, entity, comps);

        if (spawnPosition != nullptr && comps.HasComponent<TransformComponent>(entity))
        {
            auto& transform = comps.GetComponent<TransformComponent>(entity);
            transform.position = *spawnPosition;
        }

        return entity;
    }

private:
    static void WriteEntity(prefab_json& entry, const Entity entity, const ComponentManager& comps)
    {
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
            entry["mesh"] = { { "path", comps.GetComponent<MeshComponent>(entity).meshPath } };
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

        if (comps.HasComponent<ScriptComponent>(entity))
        {
            entry["script"] = { { "path", comps.GetComponent<ScriptComponent>(entity).ScriptPath } };
        }

        if (comps.HasComponent<DialogueComponent>(entity))
        {
            const auto& dialogue = comps.GetComponent<DialogueComponent>(entity);
            entry["dialogue"] = {
                { "nextEntryId", dialogue.nextEntryId },
                { "entries", prefab_json::array() }
            };

            for (const auto& dialogueEntry : dialogue.entries)
            {
                entry["dialogue"]["entries"].push_back({
                    { "id", dialogueEntry.id },
                    { "text", dialogueEntry.text }
                });
            }
        }

        if (comps.HasComponent<ColliderComponent>(entity))
        {
            const auto& collider = comps.GetComponent<ColliderComponent>(entity);
            entry["collider"] = {
                { "halfExtents", { collider.halfExtents.x, collider.halfExtents.y, collider.halfExtents.z } },
                { "isStatic", collider.isStatic }
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

        if (comps.HasComponent<AudioSourceComponent>(entity))
        {
            const auto& audio = comps.GetComponent<AudioSourceComponent>(entity);
            entry["audioSource"] = {
                { "path", audio.path },
                { "loop", audio.loop },
                { "playOnStart", audio.playOnStart },
                { "volume", audio.volume },
                { "spatial", audio.spatial },
                { "enabled", audio.enabled }
            };
        }
    }

    static void ReadEntity(const prefab_json& entry, const Entity entity, ComponentManager& comps)
    {
        if (entry.contains("transform"))
        {
            TransformComponent transform;
            const auto& tr = entry["transform"];
            transform.position = { tr["position"][0], tr["position"][1], tr["position"][2] };
            transform.rotation = { tr["rotation"][0], tr["rotation"][1], tr["rotation"][2] };
            transform.scale = { tr["scale"][0], tr["scale"][1], tr["scale"][2] };
            comps.AddComponent(entity, transform);
        }
        else
        {
            comps.AddComponent(entity, TransformComponent{});
        }

        if (entry.contains("mesh"))
        {
            MeshComponent mesh;
            mesh.meshPath = entry["mesh"].value("path", std::string{ "builtin://cube" });
            comps.AddComponent(entity, mesh);
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
            material.albedoTexture = materialEntry.value("albedoTexture", std::string{});
            material.useTexture = materialEntry.value("useTexture", false);
            comps.AddComponent(entity, material);
        }

        if (entry.contains("light"))
        {
            LightComponent light;
            const auto& lightEntry = entry["light"];
            if (lightEntry.contains("direction") && lightEntry["direction"].size() >= 3)
            {
                light.direction = { lightEntry["direction"][0], lightEntry["direction"][1], lightEntry["direction"][2] };
            }
            if (lightEntry.contains("color") && lightEntry["color"].size() >= 3)
            {
                light.color = { lightEntry["color"][0], lightEntry["color"][1], lightEntry["color"][2] };
            }
            light.intensity = lightEntry.value("intensity", light.intensity);
            light.enabled = lightEntry.value("enabled", true);
            comps.AddComponent(entity, light);
        }

        if (entry.contains("script"))
        {
            ScriptComponent script;
            script.ScriptPath = entry["script"].value("path", "");
            comps.AddComponent(entity, script);
        }

        if (entry.contains("dialogue"))
        {
            DialogueComponent dialogue;
            const auto& dialogueEntry = entry["dialogue"];
            dialogue.nextEntryId = dialogueEntry.value("nextEntryId", dialogue.nextEntryId);

            if (dialogueEntry.contains("entries"))
            {
                for (const auto& item : dialogueEntry["entries"])
                {
                    DialogueEntry value;
                    value.id = item.value("id", value.id);
                    value.text = item.value("text", std::string{});
                    dialogue.entries.push_back(value);
                    dialogue.nextEntryId = std::max(dialogue.nextEntryId, value.id + 1);
                }
            }

            comps.AddComponent(entity, dialogue);
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

        if (entry.contains("physics"))
        {
            PhysicsComponent physics;
            physics.enabled = entry["physics"].value("enabled", physics.enabled);
            physics.mass = entry["physics"].value("mass", physics.mass);
            comps.AddComponent(entity, physics);
        }

        if (entry.contains("audioSource"))
        {
            AudioSourceComponent audio;
            const auto& audioEntry = entry["audioSource"];
            audio.path = audioEntry.value("path", "");
            audio.loop = audioEntry.value("loop", false);
            audio.playOnStart = audioEntry.value("playOnStart", true);
            audio.volume = audioEntry.value("volume", 1.0f);
            audio.spatial = audioEntry.value("spatial", false);
            audio.enabled = audioEntry.value("enabled", true);
            comps.AddComponent(entity, audio);
        }
    }
};
