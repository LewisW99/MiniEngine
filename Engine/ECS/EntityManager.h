#pragma once
#include <queue>
#include <vector>
#include <cassert>
#include "Entity.h"

//Manager for creating and destroying entities
class EntityManager {
public:
    explicit EntityManager(uint32_t maxEntities = 10000);

    Entity CreateEntity();
    void DestroyEntity(Entity entity);

    bool IsAlive(Entity entity) const;

    void Clear();

    uint32_t GetMaxEntities() const { return m_MaxEntities; }
private:
    std::queue<EntityID> m_AvailableIDs;
    std::vector<bool>    m_Alive;
    uint32_t             m_MaxEntities;
    uint32_t             m_LivingCount = 0;
};
