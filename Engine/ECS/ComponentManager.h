#pragma once
#include <memory>
#include <unordered_map>
#include <typeindex>
#include "ComponentArray.h"

// ------------------------------------------------------------
// ComponentManager — owns all component arrays by type
// ------------------------------------------------------------
class ComponentManager {
public:
    template<typename T>
    void RegisterComponent() {
        std::type_index ti = typeid(T);
        if (m_ComponentArrays.find(ti) == m_ComponentArrays.end())
            m_ComponentArrays[ti] = std::make_shared<ComponentArray<T>>();
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component) {
        GetArray<T>()->InsertData(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        GetArray<T>()->RemoveData(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return GetArray<T>()->GetData(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const {
        return GetArray<T>()->GetData(entity);
    }

    template<typename T>
    std::vector<T>& GetAll() {
        return GetArray<T>()->GetRaw();
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_ComponentArrays;

    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetArray() {
        std::type_index ti = typeid(T);
        return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArrays.at(ti));
    }

    template<typename T>
    std::shared_ptr<const ComponentArray<T>> GetArray() const {
        std::type_index ti = typeid(T);
        return std::static_pointer_cast<const ComponentArray<T>>(m_ComponentArrays.at(ti));
    }
};
