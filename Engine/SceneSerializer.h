#pragma once

#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "Components/CameraFollowComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/AudioSourceComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/LightComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/NavAgentComponent.h"
#include "Components/NavWaypointComponent.h"
#include "Components/Physics/PhysicsComponent.h"
#include "Components/PlayerControllerComponent.h"
#include "Components/RuntimeUIComponent.h"
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "ECS/EntityMeta.h"
#include "Scripting/ScriptComponent.h"
#include "TransformSystem.h"

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

            if (comps.HasComponent<AnimationComponent>(entity))
            {
                const auto& animation = comps.GetComponent<AnimationComponent>(entity);
                json clip;
                clip["name"] = animation.clip.name;
                clip["duration"] = animation.clip.duration;
                clip["positionKeys"] = json::array();
                clip["rotationKeys"] = json::array();
                clip["scaleKeys"] = json::array();

                for (const auto& key : animation.clip.positionKeys)
                {
                    clip["positionKeys"].push_back({ { "time", key.time }, { "value", { key.value.x, key.value.y, key.value.z } } });
                }
                for (const auto& key : animation.clip.rotationKeys)
                {
                    clip["rotationKeys"].push_back({ { "time", key.time }, { "value", { key.value.x, key.value.y, key.value.z } } });
                }
                for (const auto& key : animation.clip.scaleKeys)
                {
                    clip["scaleKeys"].push_back({ { "time", key.time }, { "value", { key.value.x, key.value.y, key.value.z } } });
                }

                entry["animation"] = {
                    { "playing", animation.playing },
                    { "loop", animation.loop },
                    { "currentTime", animation.currentTime },
                    { "clip", clip }
                };
            }

            if (comps.HasComponent<NavWaypointComponent>(entity))
            {
                entry["navWaypoint"] = {
                    { "links", comps.GetComponent<NavWaypointComponent>(entity).links }
                };
            }

            if (comps.HasComponent<NavAgentComponent>(entity))
            {
                const auto& nav = comps.GetComponent<NavAgentComponent>(entity);
                entry["navAgent"] = {
                    { "mode", static_cast<int>(nav.mode) },
                    { "waypoints", nav.waypointEntities },
                    { "target", nav.targetEntity.id },
                    { "speed", nav.speed },
                    { "stoppingDistance", nav.stoppingDistance },
                    { "currentWaypointIndex", nav.currentWaypointIndex },
                    { "loop", nav.loop },
                    { "active", nav.active }
                };
            }

            if (comps.HasComponent<RuntimeUIComponent>(entity))
            {
                const auto& ui = comps.GetComponent<RuntimeUIComponent>(entity);
                entry["runtimeUI"] = {
                    { "type", static_cast<int>(ui.type) },
                    { "anchor", static_cast<int>(ui.anchor) },
                    { "text", ui.text },
                    { "texturePath", ui.texturePath },
                    { "buttonEvent", ui.buttonEvent },
                    { "offsetX", ui.offsetX },
                    { "offsetY", ui.offsetY },
                    { "width", ui.width },
                    { "height", ui.height },
                    { "visible", ui.visible }
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

            if (entry.contains("animation"))
            {
                AnimationComponent animation;
                const auto& animationEntry = entry["animation"];
                animation.playing = animationEntry.value("playing", false);
                animation.loop = animationEntry.value("loop", true);
                animation.currentTime = animationEntry.value("currentTime", 0.0f);

                if (animationEntry.contains("clip"))
                {
                    const auto& clipEntry = animationEntry["clip"];
                    animation.clip.name = clipEntry.value("name", std::string{ "Default" });
                    animation.clip.duration = clipEntry.value("duration", 0.0f);

                    auto readKeys = [](const json& source, std::vector<TransformKeyframe>& destination)
                    {
                        for (const auto& key : source)
                        {
                            TransformKeyframe frame;
                            frame.time = key.value("time", 0.0f);
                            if (key.contains("value") && key["value"].size() >= 3)
                            {
                                frame.value = { key["value"][0], key["value"][1], key["value"][2] };
                            }
                            destination.push_back(frame);
                        }
                    };

                    if (clipEntry.contains("positionKeys"))
                    {
                        readKeys(clipEntry["positionKeys"], animation.clip.positionKeys);
                    }
                    if (clipEntry.contains("rotationKeys"))
                    {
                        readKeys(clipEntry["rotationKeys"], animation.clip.rotationKeys);
                    }
                    if (clipEntry.contains("scaleKeys"))
                    {
                        readKeys(clipEntry["scaleKeys"], animation.clip.scaleKeys);
                    }
                }

                comps.AddComponent(entity, animation);
            }

            if (entry.contains("navWaypoint"))
            {
                NavWaypointComponent waypoint;
                waypoint.links = entry["navWaypoint"].value("links", std::vector<EntityID>{});
                comps.AddComponent(entity, waypoint);
            }

            if (entry.contains("navAgent"))
            {
                NavAgentComponent nav;
                const auto& navEntry = entry["navAgent"];
                nav.mode = static_cast<NavAgentMode>(navEntry.value("mode", 0));
                nav.waypointEntities = navEntry.value("waypoints", std::vector<EntityID>{});
                nav.targetEntity = Entity{ navEntry.value("target", uint32_t{ 0 }) };
                nav.speed = navEntry.value("speed", nav.speed);
                nav.stoppingDistance = navEntry.value("stoppingDistance", nav.stoppingDistance);
                nav.currentWaypointIndex = navEntry.value("currentWaypointIndex", nav.currentWaypointIndex);
                nav.loop = navEntry.value("loop", nav.loop);
                nav.active = navEntry.value("active", nav.active);
                comps.AddComponent(entity, nav);
            }

            if (entry.contains("runtimeUI"))
            {
                RuntimeUIComponent ui;
                const auto& uiEntry = entry["runtimeUI"];
                ui.type = static_cast<RuntimeUIElementType>(uiEntry.value("type", 0));
                ui.anchor = static_cast<RuntimeUIAnchor>(uiEntry.value("anchor", 0));
                ui.text = uiEntry.value("text", ui.text);
                ui.texturePath = uiEntry.value("texturePath", ui.texturePath);
                ui.buttonEvent = uiEntry.value("buttonEvent", ui.buttonEvent);
                ui.offsetX = uiEntry.value("offsetX", ui.offsetX);
                ui.offsetY = uiEntry.value("offsetY", ui.offsetY);
                ui.width = uiEntry.value("width", ui.width);
                ui.height = uiEntry.value("height", ui.height);
                ui.visible = uiEntry.value("visible", ui.visible);
                comps.AddComponent(entity, ui);
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
