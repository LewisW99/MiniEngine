#pragma once
#include <cstdint>


using EntityID = uint32_t;
const EntityID INVALID_ENTITY = 0xFFFFFFFF;

//Safety wrapper
struct Entity {
    EntityID id = INVALID_ENTITY;
    explicit operator bool() const { return id != INVALID_ENTITY; }
};
