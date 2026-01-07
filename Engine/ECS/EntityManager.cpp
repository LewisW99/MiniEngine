#include "pch.h"
#include "EntityManager.h"

EntityManager::EntityManager(uint32_t maxEntities)
    : m_MaxEntities(maxEntities), m_Alive(maxEntities, false)
{
    for (EntityID id = 0; id < maxEntities; ++id)
        m_AvailableIDs.push(id);
}

Entity EntityManager::CreateEntity() {
    assert(m_LivingCount < m_MaxEntities && "Too many entities!");
    EntityID id = m_AvailableIDs.front();
    m_AvailableIDs.pop();
    m_Alive[id] = true;
    m_LivingCount++;
    return Entity{ id };
}

void EntityManager::DestroyEntity(Entity entity) {
    if (!IsAlive(entity)) return;
    m_Alive[entity.id] = false;
    m_AvailableIDs.push(entity.id);
    m_LivingCount--;
}

bool EntityManager::IsAlive(Entity entity) const {
    return entity.id < m_MaxEntities && m_Alive[entity.id];
}
