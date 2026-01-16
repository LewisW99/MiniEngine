#include "pch.h"
#include "ScriptSystem.h"
#include "ScriptComponent.h"
#include "../../Demo/Editor/Managers/ProjectManager.h"
#include "../TransformSystem.h"
#include "../EditorConsole.h"
#include <iostream>
#include <optional>
#include "../InputSystem.h"
#include "../Components/Physics/PhysicsComponent.h"



extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "LuaEntity.h"




}

static ScriptSystem* s_Instance = nullptr;
static ScriptSystem* g_ScriptSystem = nullptr;

static int Lua_Translate(lua_State* L);

// Input Lua API
static int Lua_InputMoveForward(lua_State* L);
static int Lua_InputMoveRight(lua_State* L);
static int Lua_InputJumpPressed(lua_State* L);
static int Lua_InputToggleCameraPressed(lua_State* L);

// Physics Lua API
static int Lua_PhysicsSetVelocity(lua_State* L);
static int Lua_PhysicsAddImpulse(lua_State* L);
static int Lua_PhysicsIsGrounded(lua_State* L);

static PhysicsComponent* GetPhysics(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();

    if (!comps->HasComponent<PhysicsComponent>(e))
        return nullptr;

    return &comps->GetComponent<PhysicsComponent>(e);
}

static int Lua_Print(lua_State* L)
{
    int n = lua_gettop(L);
    std::string out;

    for (int i = 1; i <= n; ++i)
    {
        const char* str = lua_tostring(L, i);
        if (str)
            out += str;

        if (i < n)
            out += " ";
    }

    EditorConsole::Log(out);
    return 0;
}

static std::optional<ScriptError> ParseLuaError(
    const std::string& luaError,
    const std::string& scriptPath,
    bool runtime)
{
    // Example:
    // [string "Scripts/Player.lua"]:6: ')' expected near '0'

    ScriptError err;
    err.scriptPath = scriptPath;
    err.runtime = runtime;

    size_t linePos = luaError.find("\"]:");
    if (linePos == std::string::npos)
        return std::nullopt;

    linePos += 3;
    err.line = std::stoi(luaError.substr(linePos));

    size_t msgPos = luaError.find(":", linePos);
    if (msgPos != std::string::npos)
        err.message = luaError.substr(msgPos + 1);
    else
        err.message = luaError;

    return err;
}

static int Lua_InputPressed(lua_State* L)
{
    const char* action = luaL_checkstring(L, 1);

    bool pressed =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem &&
        g_ScriptSystem->m_InputSystem->Pressed(action);

    lua_pushboolean(L, pressed);
    return 1;
}

static int Lua_InputHeld(lua_State* L)
{
    const char* action = luaL_checkstring(L, 1);

    bool held =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem &&
        g_ScriptSystem->m_InputSystem->Held(action);

    lua_pushboolean(L, held);
    return 1;
}

static int Lua_InputReleased(lua_State* L)
{
    const char* action = luaL_checkstring(L, 1);

    bool released =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem &&
        g_ScriptSystem->m_InputSystem->Released(action);

    lua_pushboolean(L, released);
    return 1;
}

static int Lua_InputMouseDX(lua_State* L)
{
    float dx =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem
        ? g_ScriptSystem->m_InputSystem->GetMouseDX()
        : 0.0f;

    lua_pushnumber(L, dx);
    return 1;
}

static int Lua_InputMouseDY(lua_State* L)
{
    float dy =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem
        ? g_ScriptSystem->m_InputSystem->GetMouseDY()
        : 0.0f;

    lua_pushnumber(L, dy);
    return 1;
}

static int Lua_InputMoveForward(lua_State* L)
{
    float value =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem
        ? g_ScriptSystem->m_InputSystem->GetAxis("MoveZ")
        : 0.0f;

    lua_pushnumber(L, value);
    return 1;
}

static int Lua_InputMoveRight(lua_State* L)
{
    float value =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem
        ? g_ScriptSystem->m_InputSystem->GetAxis("MoveX")
        : 0.0f;

    lua_pushnumber(L, value);
    return 1;
}

static int Lua_InputJumpPressed(lua_State* L)
{
    bool pressed =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem &&
        g_ScriptSystem->m_InputSystem->Pressed("Jump");

    lua_pushboolean(L, pressed);
    return 1;
}

static int Lua_InputToggleCameraPressed(lua_State* L)
{
    bool pressed =
        g_ScriptSystem &&
        g_ScriptSystem->m_InputSystem &&
        g_ScriptSystem->m_InputSystem->Pressed("ToggleCamera");

    lua_pushboolean(L, pressed);
    return 1;
}



static int LuaTraceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else
        lua_pushliteral(L, "Unknown Lua error");

    return 1;
}

ScriptSystem& ScriptSystem::Get()
{
    return *s_Instance;
}

void ScriptSystem::Init(ComponentManager* cm)
{
    s_Instance = this;
	g_ScriptSystem = this;
    components = cm;

    m_L = luaL_newstate();
    luaL_openlibs(m_L);

    // ---------------- Transform API ----------------
    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_Translate);
    lua_setfield(m_L, -2, "Translate");
    lua_setglobal(m_L, "Transform");

    lua_pushcfunction(m_L, Lua_Print);
    lua_setglobal(m_L, "print");

    // ---------------- Input API ----------------
    lua_newtable(m_L);               

    lua_pushcfunction(m_L, Lua_InputMouseDX);
    lua_setfield(m_L, -2, "MouseDX");

    lua_pushcfunction(m_L, Lua_InputMouseDY);
    lua_setfield(m_L, -2, "MouseDY");

    lua_pushcfunction(m_L, Lua_InputPressed);
    lua_setfield(m_L, -2, "Pressed");

    lua_pushcfunction(m_L, Lua_InputHeld);
    lua_setfield(m_L, -2, "Held");

    lua_pushcfunction(m_L, Lua_InputReleased);
    lua_setfield(m_L, -2, "Released");

    lua_pushcfunction(m_L, Lua_InputMoveForward);
    lua_setfield(m_L, -2, "MoveForward");

    lua_pushcfunction(m_L, Lua_InputMoveRight);
    lua_setfield(m_L, -2, "MoveRight");

    lua_pushcfunction(m_L, Lua_InputJumpPressed);
    lua_setfield(m_L, -2, "JumpPressed");

    lua_pushcfunction(m_L, Lua_InputToggleCameraPressed);
    lua_setfield(m_L, -2, "ToggleCameraPressed");

    lua_setglobal(m_L, "Input");

    lua_newtable(m_L);

    lua_pushcfunction(m_L, Lua_PhysicsSetVelocity);
    lua_setfield(m_L, -2, "SetVelocity");

    lua_pushcfunction(m_L, Lua_PhysicsAddImpulse);
    lua_setfield(m_L, -2, "AddImpulse");

    lua_pushcfunction(m_L, Lua_PhysicsIsGrounded);
    lua_setfield(m_L, -2, "IsGrounded");

    lua_setglobal(m_L, "Physics");
    //lua_pop(m_L, 1);
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
        return LUA_REFNIL;
    }

    return luaL_ref(m_L, LUA_REGISTRYINDEX);
}

void ScriptSystem::LoadScript(ScriptComponent& script)
{
    if (!m_L)
    {
        EditorConsole::Error("[ScriptSystem] Lua state is NULL");
        return;
    }

    if (!ProjectManager::HasActiveProject())
    {
        EditorConsole::Error("[ScriptSystem] No active project");
        return;
    }

    const auto& project = ProjectManager::GetActive();

    std::filesystem::path fullPath =
        project.rootPath / script.ScriptPath;

    if (!std::filesystem::exists(fullPath))
    {
        EditorConsole::Error(
            "[Lua] Script not found: " + fullPath.string());
        return;
    }

    if (luaL_dofile(m_L, fullPath.string().c_str()) != LUA_OK)
    {
        const char* err = lua_tostring(m_L, -1);
        std::string errStr = err ? err : "[Lua] Unknown script error";

        if (auto parsed = ParseLuaError(errStr, script.ScriptPath, false))
        {
            EditorConsole::Error(
                errStr,
                parsed->scriptPath,
                parsed->line
            );

            m_Errors.push_back(*parsed);
        }
        else
        {
            EditorConsole::Error(errStr);
        }

        lua_pop(m_L, 1);
        return;
    }

    script.OnStart = GetFunctionRef("OnStart");
    script.OnUpdate = GetFunctionRef("OnUpdate");
    script.OnDestroy = GetFunctionRef("OnDestroy");
    script.Started = false;

    EditorConsole::Log(
        "[Lua] Loaded script: " + script.ScriptPath);
}


void ScriptSystem::CallFunction(int fnRef, Entity entity, float dt)
{
    if (fnRef == LUA_REFNIL)
        return;

    // Push error handler
    lua_pushcfunction(m_L, LuaTraceback);
    int errFunc = lua_gettop(m_L);

    // Push function
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, fnRef);

    // Push self
    LuaEntity* le = (LuaEntity*)lua_newuserdata(m_L, sizeof(LuaEntity));
    le->entity = entity;

    // Push dt
    lua_pushnumber(m_L, dt);

    // Call function with error handler
    if (lua_pcall(m_L, 2, 0, errFunc) != LUA_OK)
    {
        const char* err = lua_tostring(m_L, -1);
        std::string errStr = err ? err : "[Lua] Runtime error";

        if (auto parsed = ParseLuaError(errStr, "<runtime>", true))
        {
            EditorConsole::Error(
                errStr,
                parsed->scriptPath,
                parsed->line
            );

            m_Errors.push_back(*parsed);
        }
        else
        {
            EditorConsole::Error(errStr);
        }

        lua_pop(m_L, 1);
    }

    // Remove error handler
    lua_remove(m_L, errFunc);
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

bool ScriptSystem::ValidateScriptText(
    const std::string& text,
    std::string& outError)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_loadstring(L, text.c_str()) != LUA_OK)
    {
        outError = lua_tostring(L, -1);

        

        lua_close(L);
        return false;
    }

    lua_close(L);
    return true;
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

static int Lua_PhysicsSetVelocity(lua_State* L)
{
    LuaEntity* le = (LuaEntity*)lua_touserdata(L, 1);
    if (!le) return 0;

    float x = (float)luaL_optnumber(L, 2, 0.0f);
    float y = (float)luaL_optnumber(L, 3, 0.0f);
    float z = (float)luaL_optnumber(L, 4, 0.0f);

    auto* phys = GetPhysics(le->entity);
    if (!phys) return 0;

    phys->velocity = { x, y, z };
    return 0;
}

static int Lua_PhysicsAddImpulse(lua_State* L)
{
    LuaEntity* le = (LuaEntity*)lua_touserdata(L, 1);
    if (!le) return 0;

    float x = (float)luaL_optnumber(L, 2, 0.0f);
    float y = (float)luaL_optnumber(L, 3, 0.0f);
    float z = (float)luaL_optnumber(L, 4, 0.0f);

    auto* phys = GetPhysics(le->entity);
    if (!phys) return 0;

    phys->velocity.x += x;
    phys->velocity.y += y;
    phys->velocity.z += z;

    return 0;
}

static int Lua_PhysicsIsGrounded(lua_State* L)
{
    LuaEntity* le = (LuaEntity*)lua_touserdata(L, 1);
    if (!le)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    auto* phys = GetPhysics(le->entity);
    lua_pushboolean(L, phys ? phys->grounded : false);
    return 1;
}


void ScriptSystem::SetInputSystem(InputSystem* input)
{
    m_InputSystem = input;
}


