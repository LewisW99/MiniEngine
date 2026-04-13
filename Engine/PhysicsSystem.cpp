#include "pch.h"
#include "PhysicsSystem.h"

#include "Components/ColliderComponent.h"
#include "Components/TagComponent.h"
#include "TransformSystem.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include "TransformSystem.h"
#include "Components/Physics/PhysicsComponent.h"

static constexpr float Gravity = -9.81f;
static constexpr float GroundHeight = 0.0f;

static bool AABBOverlap(
    const Vec3& aPos,
    const Vec3& aHalf,
    const Vec3& bPos,
    const Vec3& bHalf)
{
    return
        std::abs(aPos.x - bPos.x) <= (aHalf.x + bHalf.x) &&
        std::abs(aPos.y - bPos.y) <= (aHalf.y + bHalf.y) &&
        std::abs(aPos.z - bPos.z) <= (aHalf.z + bHalf.z);
}

static bool CanCollide(const ColliderComponent& a, const ColliderComponent& b)
{
    return (a.mask & b.layer) != 0u && (b.mask & a.layer) != 0u;
}

static void RecordOverlap(
    ComponentManager& comps,
    const Entity entity,
    const Entity other,
    const bool isTriggerPair)
{
    if (!comps.HasComponent<PhysicsComponent>(entity))
    {
        return;
    }

    auto& physics = comps.GetComponent<PhysicsComponent>(entity);
    physics.overlappingEntities.push_back(other.id);

    if (comps.HasComponent<TagComponent>(other))
    {
        const auto& tag = comps.GetComponent<TagComponent>(other);
        if (!tag.tag.empty())
        {
            physics.overlappingTags.push_back(tag.tag);
            if (isTriggerPair)
            {
                physics.triggerTags.push_back(tag.tag);
            }
        }
    }

    if (isTriggerPair)
    {
        physics.triggerEntities.push_back(other.id);
    }
}

void PhysicsSystem::Update(
    EntityManager& entities,
    ComponentManager& comps,
    float dt
)
{
    const uint32_t maxEntities = entities.GetMaxEntities();

    for (uint32_t id = 0; id < maxEntities; ++id)
    {
        Entity e{ id };

        if (!entities.IsAlive(e))
            continue;

        if (!comps.HasComponent<PhysicsComponent>(e) ||
            !comps.HasComponent<TransformComponent>(e))
            continue;

        auto& physics =
            comps.GetComponent<PhysicsComponent>(e);

        physics.overlappingEntities.clear();
        physics.overlappingTags.clear();
        physics.triggerEntities.clear();
        physics.triggerTags.clear();

        auto& transform =
            comps.GetComponent<TransformComponent>(e);

        if (!physics.enabled)
            continue;

        // ------------------------------------------------------------
        // APPLY GRAVITY
        // ------------------------------------------------------------
        if (!physics.grounded)
            physics.velocity.y += Gravity * dt;

        // ------------------------------------------------------------
        // INTEGRATE VELOCITY
        // ------------------------------------------------------------
        transform.position.x += physics.velocity.x * dt;
        transform.position.y += physics.velocity.y * dt;
        transform.position.z += physics.velocity.z * dt;

        // ------------------------------------------------------------
        // GROUND CLAMP
        // ------------------------------------------------------------
        if (transform.position.y <= GroundHeight)
        {
            transform.position.y = GroundHeight;
            physics.velocity.y = 0.0f;
            physics.grounded = true;
        }
        else
        {
            physics.grounded = false;
        }

        // ------------------------------------------------------------
        // COLLISIONS
        // ------------------------------------------------------------
        if (!comps.HasComponent<ColliderComponent>(e))
            continue;

        auto& colA =
            comps.GetComponent<ColliderComponent>(e);

        if (colA.isStatic || colA.mode == CollisionMode::None)
            continue;

        for (uint32_t otherID = 0; otherID < maxEntities; ++otherID)
        {
            if (otherID == id)
                continue;

            Entity other{ otherID };

            if (!entities.IsAlive(other))
                continue;

            if (!comps.HasComponent<ColliderComponent>(other) ||
                !comps.HasComponent<TransformComponent>(other))
                continue;

            auto& colB =
                comps.GetComponent<ColliderComponent>(other);

            if (colB.mode == CollisionMode::None)
                continue;

            if (!CanCollide(colA, colB))
                continue;

            if (!colB.isStatic && otherID < id)
                continue;

            auto& otherTransform =
                comps.GetComponent<TransformComponent>(other);

            if (!AABBOverlap(
                transform.position, colA.halfExtents,
                otherTransform.position, colB.halfExtents))
                continue;

            const bool isTriggerPair =
                colA.mode == CollisionMode::Trigger ||
                colB.mode == CollisionMode::Trigger;
            const bool isBlockingPair =
                colA.mode == CollisionMode::Blocking &&
                colB.mode == CollisionMode::Blocking;

            RecordOverlap(comps, e, other, isTriggerPair);
            RecordOverlap(comps, other, e, isTriggerPair);

            if (!isBlockingPair || isTriggerPair)
                continue;

            // --------------------------------------------------------
            // PENETRATION DEPTH (component-wise)
            // --------------------------------------------------------
            float dx = transform.position.x - otherTransform.position.x;
            float px = (colA.halfExtents.x + colB.halfExtents.x) - std::abs(dx);

            float dy = transform.position.y - otherTransform.position.y;
            float py = (colA.halfExtents.y + colB.halfExtents.y) - std::abs(dy);

            float dz = transform.position.z - otherTransform.position.z;
            float pz = (colA.halfExtents.z + colB.halfExtents.z) - std::abs(dz);

            // Resolve smallest penetration axis
            if (px < py && px < pz)
            {
                transform.position.x += (dx < 0.0f ? -px : px);
                physics.velocity.x = 0.0f;
            }
            else if (py < pz)
            {
                transform.position.y += (dy < 0.0f ? -py : py);
                physics.velocity.y = 0.0f;

                if (dy > 0.0f)
                    physics.grounded = true;
            }
            else
            {
                transform.position.z += (dz < 0.0f ? -pz : pz);
                physics.velocity.z = 0.0f;
            }
        }
    }
}
