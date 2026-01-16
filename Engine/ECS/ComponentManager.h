#pragma once
#include <memory>
#include <unordered_map>
#include <typeindex>
#include "ComponentArray.h"
#include <iostream>

// ------------------------------------------------------------
// ComponentManager — owns all component arrays by type
// ------------------------------------------------------------
class ComponentManager {
public:
    template<typename T>
    void RegisterComponent(const char* debugName = nullptr)
    {
        std::type_index ti = typeid(T);

        if (m_ComponentArrays.find(ti) != m_ComponentArrays.end())
        {
            std::cerr
                << "[ComponentManager] "
                << (debugName ? debugName : ti.name())
                << " already registered\n";
            return;
        }

        std::cerr
            << "[ComponentManager] Registering "
            << (debugName ? debugName : ti.name())
            << "...\n";

        m_ComponentArrays[ti] =
            std::make_shared<ComponentArray<T>>();

        std::cerr
            << "[ComponentManager] "
            << (debugName ? debugName : ti.name())
            << " registered OK\n";
    }

    template<typename T>
    void AddComponent(Entity entity, const T& component)
    {
        GetArray<T>()->InsertData(entity, component);
    }

    template<typename T>
    void RemoveComponent(Entity entity)
    {
        GetArray<T>()->RemoveData(entity);
    }

    template<typename T>
    T& GetComponent(Entity entity)
    {
        return GetArray<T>()->GetData(entity);
    }

    template<typename T>
    const T& GetComponent(Entity entity) const
    {
        return GetArray<T>()->GetData(entity);
    }

    template<typename T>
    std::vector<T>& GetAll()
    {
        return GetArray<T>()->GetRaw();
    }

    // Clears ALL component data (used for Play Mode transitions)
    void Clear()
    {
        for (auto& [type, array] : m_ComponentArrays)
            array->Clear();
    }

    template<typename T>
    bool HasComponent(Entity entity) const
    {
        auto it = m_ComponentArrays.find(std::type_index(typeid(T)));
        if (it == m_ComponentArrays.end())
            return false;

        auto array =
            static_cast<const ComponentArray<T>*>(it->second.get());

        return array->HasData(entity);
    }
    template<typename T>
    bool IsComponentRegistered() const
    {
        return m_ComponentArrays.find(
            std::type_index(typeid(T))
        ) != m_ComponentArrays.end();
    }

    void DumpRegisteredComponents() const
    {
        std::cerr << "---- Registered Components ----\n";

        for (const auto& [type, array] : m_ComponentArrays)
        {
            std::cerr << "  " << type.name() << "\n";
        }

        std::cerr << "--------------------------------\n";
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_ComponentArrays;

    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetArray()
    {
        return std::static_pointer_cast<ComponentArray<T>>(
            m_ComponentArrays.at(std::type_index(typeid(T)))
        );
    }

    template<typename T>
    std::shared_ptr<const ComponentArray<T>> GetArray() const
    {
        return std::static_pointer_cast<const ComponentArray<T>>(
            m_ComponentArrays.at(std::type_index(typeid(T)))
        );
    }
};
