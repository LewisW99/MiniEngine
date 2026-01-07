#pragma once
#include <unordered_map>
#include <vector>
#include <cassert>
#include "../ECS/Entity.h"

// ------------------------------------------------------------
// ComponentArray<T> — tightly-packed storage for one component type
// ------------------------------------------------------------
template<typename T>
class ComponentArray {
public:
    void InsertData(Entity entity, const T& component) {
        assert(m_EntityToIndex.find(entity.id) == m_EntityToIndex.end() && "Component already exists!");
        size_t index = m_Size;
        m_EntityToIndex[entity.id] = index;
        if (index >= m_Components.size()) m_Components.resize(index + 1);
        m_Components[index] = component;
        m_IndexToEntity[index] = entity.id;
        ++m_Size;
    }

    void RemoveData(Entity entity) {
        assert(m_EntityToIndex.find(entity.id) != m_EntityToIndex.end() && "Component does not exist!");
        size_t removedIndex = m_EntityToIndex[entity.id];
        size_t lastIndex = m_Size - 1;
        m_Components[removedIndex] = m_Components[lastIndex];
        EntityID lastEntity = m_IndexToEntity[lastIndex];
        m_EntityToIndex[lastEntity] = removedIndex;
        m_IndexToEntity[removedIndex] = lastEntity;
        m_EntityToIndex.erase(entity.id);
        m_IndexToEntity.erase(lastIndex);
        --m_Size;
    }

    T& GetData(Entity entity) {
        assert(m_EntityToIndex.find(entity.id) != m_EntityToIndex.end() && "Component does not exist!");
        return m_Components[m_EntityToIndex[entity.id]];
    }

    const T& GetData(Entity entity) const {
        return m_Components[m_EntityToIndex.at(entity.id)];
    }

    bool HasData(Entity entity) const {
        return m_EntityToIndex.find(entity.id) != m_EntityToIndex.end();
    }

    std::vector<T>& GetRaw() { return m_Components; }

private:
    std::vector<T> m_Components;
    std::unordered_map<EntityID, size_t> m_EntityToIndex;
    std::unordered_map<size_t, EntityID> m_IndexToEntity;
    size_t m_Size = 0;
};
