#pragma once
#include <string>
#include "Entity.h"
#include <unordered_map>

// ------------------------------------------------------------
// EntityMeta - stores editor-facing info (name, etc.)
// ------------------------------------------------------------
class EntityMeta {
public:
    std::unordered_map<EntityID, std::string> names;

    std::string& GetName(Entity e) {
        return names[e.id];
    }

    void SetName(Entity e, const std::string& newName) {
        names[e.id] = newName;
    }

    void Remove(Entity e) {
        names.erase(e.id);
    }

    void Clear() {
        names.clear();
	}
};