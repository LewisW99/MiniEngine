#include "pch.h"
#include "ScriptSystem.h"
#include "ScriptComponent.h"
#include "../../Demo/Editor/Managers/ProjectManager.h"
#include "../TransformSystem.h"
#include <iostream>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "LuaEntity.h"

}

static ScriptSystem* s_Instance = nullptr;

static int Lua_Translate(lua_State* L);

ScriptSystem& ScriptSystem::Get()
{
    return *s_Instance;
}

void ScriptSystem::Init(ComponentManager* cm)
{
    s_Instance = this;
    components = cm;

    m_L = luaL_newstate();
    luaL_openlibs(m_L);

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_Translate);
    lua_setfield(m_L, -2, "Translate");
    lua_setglobal(m_L, "Transform");
}

void ScriptSystem::Shutdown()
{
    lua_close(m_L);
}

int ScriptSystem::GetFunctionRef(const char* name)
{
    lua_getglobal(m_L, name);

    if (!lua_isfunction(m_L, -1))
    {
        lua_pop(m_L, 1);
        return -1;
    }

    return luaL_ref(m_L, LUA_REGISTRYINDEX);
}

void ScriptSystem::LoadScript(ScriptComponent& script)
{
    if (!m_L)
    {
        std::cerr << "[ScriptSystem] Lua state is NULL\n";
        return;
    }

    if (!ProjectManager::HasActiveProject())
    {
        std::cerr << "[ScriptSystem] No active project\n";
        return;
    }

    const auto& project = ProjectManager::GetActive();

    std::filesystem::path fullPath =
        project.rootPath / script.ScriptPath;

    if (!std::filesystem::exists(fullPath))
    {
        std::cerr << "[Lua] Script not found: "
            << fullPath.string() << "\n";
        return;
    }

    if (luaL_dofile(m_L, fullPath.string().c_str()) != LUA_OK)
    {
        std::cerr << "[Lua] "
            << lua_tostring(m_L, -1) << "\n";
        lua_pop(m_L, 1);
        return;
    }

    script.OnStart = GetFunctionRef("OnStart");
    script.OnUpdate = GetFunctionRef("OnUpdate");
    script.OnDestroy = GetFunctionRef("OnDestroy");
    script.Started = false;
}

void ScriptSystem::CallFunction(int fnRef, Entity entity, float dt)
{
    if (fnRef == -1) return;

    lua_rawgeti(m_L, LUA_REGISTRYINDEX, fnRef);

    // push self
    LuaEntity* le = (LuaEntity*)lua_newuserdata(m_L, sizeof(LuaEntity));
    le->entity = entity;

    // push dt
    lua_pushnumber(m_L, dt);

    if (lua_pcall(m_L, 2, 0, 0) != LUA_OK)
    {
        std::cerr << "[Lua] " << lua_tostring(m_L, -1) << "\n";
        lua_pop(m_L, 1);
    }
}

void ScriptSystem::Update(Entity entity, ScriptComponent& script, float dt)
{
    if (!script.Started)
    {
        CallFunction(script.OnStart, entity, 0.0f);
        script.Started = true;
    }

    CallFunction(script.OnUpdate, entity, dt);
}

static int Lua_Translate(lua_State* L)
{
    // arg 1 = self
    LuaEntity* le = (LuaEntity*)lua_touserdata(L, 1);
    if (!le) return 0;

    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    Entity e = le->entity;

    // Access TransformComponent directly
    if (!ScriptSystem::Get().GetComponents()->HasComponent<TransformComponent>(e))
        return 0;

    auto& t =
        ScriptSystem::Get().GetComponents()->GetComponent<TransformComponent>(e);

    t.position.x += x;
    t.position.y += y;
    t.position.z += z;

    return 0;
}


