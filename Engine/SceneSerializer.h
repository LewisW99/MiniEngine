#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/ECS/EntityMeta.h"
#include "../Engine/Components/Physics/PhysicsComponent.h"
#include "../Engine/Components/PlayerControllerComponent.h"
#include "../Engine/Components/ColliderComponent.h"
#include "../Engine/Components/CameraFollowComponent.h"
#include "../Engine/Scripting/ScriptComponent.h"

using json = nlohmann::json;

// ------------------------------------------------------------
// SceneSerializer - handles saving/loading ECS state
// ------------------------------------------------------------
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

        for (uint32_t id = 0; id < 10000; ++id)
        {
            Entity e{ id };
            if (!entities.IsAlive(e)) continue;

            json entry;
            entry["id"] = e.id;
            entry["name"] = meta.names.count(e.id)
                ? meta.names.at(e.id)
                : "Entity " + std::to_string(e.id);

            // -------- Transform --------
            if (comps.HasComponent<TransformComponent>(e))
            {
                const auto& t = comps.GetComponent<TransformComponent>(e);
                entry["transform"] = {
                    { "position", { t.position.x, t.position.y, t.position.z } },
                    { "rotation", { t.rotation.x, t.rotation.y, t.rotation.z } },
                    { "scale",    { t.scale.x,    t.scale.y,    t.scale.z } }
                };
            }

            // -------- Physics --------
            if (comps.HasComponent<PhysicsComponent>(e))
            {
                const auto& p = comps.GetComponent<PhysicsComponent>(e);
                entry["physics"] = {
                    { "enabled", p.enabled },
                    { "mass",    p.mass }
                };
            }

            if (comps.HasComponent<ColliderComponent>(e))
            {
                const auto& c = comps.GetComponent<ColliderComponent>(e);
                entry["collider"] = {
                    { "halfExtents", { c.halfExtents.x, c.halfExtents.y, c.halfExtents.z } },
                    { "isStatic", c.isStatic }
                };
            }

            if (comps.HasComponent<ScriptComponent>(e))
            {
                const auto& s = comps.GetComponent<ScriptComponent>(e);
                entry["script"] = {
                    { "path", s.ScriptPath }
                };
            }

            if (comps.HasComponent<PlayerControllerComponent>(e))
            {
                const auto& pc = comps.GetComponent<PlayerControllerComponent>(e);
                entry["playerController"] = {
                    { "moveSpeed", pc.moveSpeed },
                    { "lookSpeed", pc.lookSpeed }
                };
            }

            // -------- Camera Follow --------
            if (comps.HasComponent<CameraFollowComponent>(e))
            {
                const auto& c = comps.GetComponent<CameraFollowComponent>(e);
                entry["cameraFollow"] = {
                    { "target",  c.target.id },
                    { "distance", c.distance },
                    { "height", c.height },
                    { "smoothness", c.smoothness }
                };
            }


            root["entities"].push_back(entry);
        }

        std::ofstream file(path);
        file << root.dump(4);
        file.close();
    }

    static void Load(const std::string& path,
        EntityManager& entities,
        ComponentManager& comps,
        EntityMeta& meta)
    {
        std::ifstream file(path);
        if (!file.is_open()) return;

        json root;
        file >> root;
        file.close();

        // Clear existing ECS
        entities.Clear();
        comps.Clear();
        meta.Clear();

        for (auto& entry : root["entities"])
        {
            Entity e = entities.CreateEntity();
            meta.SetName(e, entry.value("name", "Entity " + std::to_string(e.id)));

            // -------- Transform --------
            if (entry.contains("transform"))
            {
                TransformComponent t;
                auto& tr = entry["transform"];

                t.position = { tr["position"][0], tr["position"][1], tr["position"][2] };
                t.rotation = { tr["rotation"][0], tr["rotation"][1], tr["rotation"][2] };
                t.scale = { tr["scale"][0],    tr["scale"][1],    tr["scale"][2] };

                comps.AddComponent(e, t);
            }

            // -------- Physics --------
            if (entry.contains("physics"))
            {
                PhysicsComponent p;
                p.enabled = entry["physics"].value("enabled", true);
                p.mass = entry["physics"].value("mass", 1.0f);

                comps.AddComponent(e, p);
            }

            if (entry.contains("collider"))
            {
                ColliderComponent c;
                auto& col = entry["collider"];

                c.halfExtents = {
                    col["halfExtents"][0],
                    col["halfExtents"][1],
                    col["halfExtents"][2]
                };

                c.isStatic = col.value("isStatic", false);

                comps.AddComponent(e, c);
            }

            if (entry.contains("script"))
            {
                ScriptComponent s;
                s.ScriptPath = entry["script"].value("path", "");

                comps.AddComponent(e, s);
            }

            if (entry.contains("playerController"))
            {
                PlayerControllerComponent pc;
                pc.moveSpeed = entry["playerController"].value("moveSpeed", 5.0f);
                pc.lookSpeed = entry["playerController"].value("lookSpeed", 0.1f);

                comps.AddComponent(e, pc);
            }

            // -------- Camera Follow --------
            if (entry.contains("cameraFollow"))
            {
                CameraFollowComponent c;
                uint32_t targetId =
                    entry["cameraFollow"].value("target", uint32_t{ 0 });

                c.target = Entity{ targetId };
                c.distance = entry["cameraFollow"].value("distance", 5.0f);
                c.height = entry["cameraFollow"].value("height", 2.0f);
                c.smoothness = entry["cameraFollow"].value("smoothness", 10.0f);

                comps.AddComponent(e, c);
            }

        }
    }
};