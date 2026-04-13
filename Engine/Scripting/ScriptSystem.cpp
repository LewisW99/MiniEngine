#include "pch.h"
#include "ScriptSystem.h"
#include "ScriptComponent.h"
#include "../../Demo/Editor/Managers/ProjectManager.h"
#include "../ECS/EntityManager.h"
#include "../TransformSystem.h"
#include "../EditorConsole.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include "../InputSystem.h"
#include "../Components/ColliderComponent.h"
#include "../Components/DialogueComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/MaterialComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/RuntimeUIComponent.h"
#include "../Components/TagComponent.h"
#include "../Components/Physics/PhysicsComponent.h"
#include "../Components/PlayerControllerComponent.h"
#include "../Components/CameraFollowComponent.h"
#include "../Components/AudioSourceComponent.h"
#include "../Systems/AudioSystem.h"
#include "../EventBus.h"



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
static int Lua_GetPosition(lua_State* L);
static int Lua_SetPosition(lua_State* L);
static int Lua_GetRotation(lua_State* L);
static int Lua_SetRotation(lua_State* L);
static int Lua_Rotate(lua_State* L);

// Input Lua API
static int Lua_InputMoveForward(lua_State* L);
static int Lua_InputMoveRight(lua_State* L);
static int Lua_InputJumpPressed(lua_State* L);
static int Lua_InputToggleCameraPressed(lua_State* L);

// Physics Lua API
static int Lua_PhysicsSetVelocity(lua_State* L);
static int Lua_PhysicsAddImpulse(lua_State* L);
static int Lua_PhysicsIsGrounded(lua_State* L);
static int Lua_PhysicsSetEnabled(lua_State* L);
static int Lua_PhysicsIsEnabled(lua_State* L);
static int Lua_PhysicsIsTouchingTag(lua_State* L);

static int Lua_EntityDestroy(lua_State* L);
static int Lua_EntityHasComponent(lua_State* L);
static int Lua_EntitySpawn(lua_State* L);
static int Lua_EntityGetTag(lua_State* L);
static int Lua_EntitySetTag(lua_State* L);

static int Lua_MaterialGetAlbedo(lua_State* L);
static int Lua_MaterialSetAlbedo(lua_State* L);
static int Lua_MaterialGetSpecular(lua_State* L);
static int Lua_MaterialSetSpecular(lua_State* L);
static int Lua_MaterialGetShininess(lua_State* L);
static int Lua_MaterialSetShininess(lua_State* L);
static int Lua_MaterialGetTexture(lua_State* L);
static int Lua_MaterialSetTexture(lua_State* L);
static int Lua_MaterialUseTexture(lua_State* L);
static int Lua_MaterialSetUseTexture(lua_State* L);

static int Lua_LightGetDirection(lua_State* L);
static int Lua_LightSetDirection(lua_State* L);
static int Lua_LightGetColor(lua_State* L);
static int Lua_LightSetColor(lua_State* L);
static int Lua_LightGetIntensity(lua_State* L);
static int Lua_LightSetIntensity(lua_State* L);
static int Lua_AudioPlay(lua_State* L);
static int Lua_AudioStop(lua_State* L);
static int Lua_AudioPlayOneShot(lua_State* L);
static int Lua_SceneLoadByBuildIndex(lua_State* L);
static int Lua_SceneLoadByName(lua_State* L);
static int Lua_DialogueGetEntryCount(lua_State* L);
static int Lua_DialogueGetEntryByIndex(lua_State* L);
static int Lua_DialogueGetEntryText(lua_State* L);

static PhysicsComponent* GetPhysics(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();

    if (!comps->HasComponent<PhysicsComponent>(e))
        return nullptr;

    return &comps->GetComponent<PhysicsComponent>(e);
}

static LuaEntity* GetLuaEntity(lua_State* L, const int index)
{
    return static_cast<LuaEntity*>(lua_touserdata(L, index));
}

static bool IsEntityValid(const Entity entity)
{
    auto* entities = ScriptSystem::Get().GetEntities();
    return entities != nullptr && entities->IsAlive(entity);
}

static TransformComponent* GetTransform(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<TransformComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<TransformComponent>(e);
}

static MaterialComponent* GetMaterial(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<MaterialComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<MaterialComponent>(e);
}

static TagComponent* GetTag(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<TagComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<TagComponent>(e);
}

static LightComponent* GetLight(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<LightComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<LightComponent>(e);
}

static AudioSourceComponent* GetAudioSource(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<AudioSourceComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<AudioSourceComponent>(e);
}

static DialogueComponent* GetDialogue(Entity e)
{
    auto* comps = ScriptSystem::Get().GetComponents();
    if (comps == nullptr || !comps->HasComponent<DialogueComponent>(e))
    {
        return nullptr;
    }

    return &comps->GetComponent<DialogueComponent>(e);
}

static Entity GetEntityArg(lua_State* L, const int index, bool* valid = nullptr)
{
    const LuaEntity* luaEntity = GetLuaEntity(L, index);
    const Entity entity = luaEntity != nullptr ? luaEntity->entity : Entity{};
    const bool isValid = luaEntity != nullptr && IsEntityValid(entity);

    if (valid != nullptr)
    {
        *valid = isValid;
    }

    return entity;
}

static void PushEntity(lua_State* L, const Entity entity)
{
    auto* luaEntity = static_cast<LuaEntity*>(lua_newuserdata(L, sizeof(LuaEntity)));
    luaEntity->entity = entity;
}

static void PushVec3(lua_State* L, const float x, const float y, const float z)
{
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
}

template<typename T>
static void RemoveComponentIfPresent(ComponentManager& components, const Entity entity)
{
    if (components.HasComponent<T>(entity))
    {
        components.RemoveComponent<T>(entity);
    }
}

static void DestroyKnownComponents(ComponentManager& components, const Entity entity)
{
    RemoveComponentIfPresent<TransformComponent>(components, entity);
    RemoveComponentIfPresent<MeshComponent>(components, entity);
    RemoveComponentIfPresent<MaterialComponent>(components, entity);
    RemoveComponentIfPresent<LightComponent>(components, entity);
    RemoveComponentIfPresent<TagComponent>(components, entity);
    RemoveComponentIfPresent<PhysicsComponent>(components, entity);
    RemoveComponentIfPresent<ColliderComponent>(components, entity);
    RemoveComponentIfPresent<RuntimeUIComponent>(components, entity);
    RemoveComponentIfPresent<ScriptComponent>(components, entity);
    RemoveComponentIfPresent<AudioSourceComponent>(components, entity);
    RemoveComponentIfPresent<PlayerControllerComponent>(components, entity);
    RemoveComponentIfPresent<CameraFollowComponent>(components, entity);
}

static bool HasNamedComponent(ComponentManager& components, const Entity entity, const std::string& name)
{
    if (name == "TransformComponent" || name == "Transform")
    {
        return components.HasComponent<TransformComponent>(entity);
    }
    if (name == "MeshComponent" || name == "Mesh")
    {
        return components.HasComponent<MeshComponent>(entity);
    }
    if (name == "MaterialComponent" || name == "Material")
    {
        return components.HasComponent<MaterialComponent>(entity);
    }
    if (name == "LightComponent" || name == "Light")
    {
        return components.HasComponent<LightComponent>(entity);
    }
    if (name == "TagComponent" || name == "Tag")
    {
        return components.HasComponent<TagComponent>(entity);
    }
    if (name == "PhysicsComponent" || name == "Physics")
    {
        return components.HasComponent<PhysicsComponent>(entity);
    }
    if (name == "ColliderComponent" || name == "Collider")
    {
        return components.HasComponent<ColliderComponent>(entity);
    }
    if (name == "ScriptComponent" || name == "Script")
    {
        return components.HasComponent<ScriptComponent>(entity);
    }
    if (name == "RuntimeUIComponent" || name == "RuntimeUI" || name == "UI")
    {
        return components.HasComponent<RuntimeUIComponent>(entity);
    }
    if (name == "AudioSourceComponent" || name == "AudioSource" || name == "Audio")
    {
        return components.HasComponent<AudioSourceComponent>(entity);
    }

    return false;
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

void ScriptSystem::Init(ComponentManager* cm, EntityManager* em)
{
    s_Instance = this;
	g_ScriptSystem = this;
    components = cm;
    entityManager = em;

    m_L = luaL_newstate();
    luaL_openlibs(m_L);

    // ---------------- Transform API ----------------
    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_GetPosition);
    lua_setfield(m_L, -2, "GetPosition");
    lua_pushcfunction(m_L, Lua_SetPosition);
    lua_setfield(m_L, -2, "SetPosition");
    lua_pushcfunction(m_L, Lua_GetRotation);
    lua_setfield(m_L, -2, "GetRotation");
    lua_pushcfunction(m_L, Lua_SetRotation);
    lua_setfield(m_L, -2, "SetRotation");
    lua_pushcfunction(m_L, Lua_Rotate);
    lua_setfield(m_L, -2, "Rotate");
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
    lua_pushcfunction(m_L, Lua_PhysicsSetEnabled);
    lua_setfield(m_L, -2, "SetEnabled");
    lua_pushcfunction(m_L, Lua_PhysicsIsEnabled);
    lua_setfield(m_L, -2, "IsEnabled");
    lua_pushcfunction(m_L, Lua_PhysicsIsTouchingTag);
    lua_setfield(m_L, -2, "IsTouchingTag");

    lua_setglobal(m_L, "Physics");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_EntityDestroy);
    lua_setfield(m_L, -2, "Destroy");
    lua_pushcfunction(m_L, Lua_EntityHasComponent);
    lua_setfield(m_L, -2, "HasComponent");
    lua_pushcfunction(m_L, Lua_EntitySpawn);
    lua_setfield(m_L, -2, "Spawn");
    lua_pushcfunction(m_L, Lua_EntityGetTag);
    lua_setfield(m_L, -2, "GetTag");
    lua_pushcfunction(m_L, Lua_EntitySetTag);
    lua_setfield(m_L, -2, "SetTag");
    lua_setglobal(m_L, "Entity");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_MaterialGetAlbedo);
    lua_setfield(m_L, -2, "GetAlbedo");
    lua_pushcfunction(m_L, Lua_MaterialSetAlbedo);
    lua_setfield(m_L, -2, "SetAlbedo");
    lua_pushcfunction(m_L, Lua_MaterialGetSpecular);
    lua_setfield(m_L, -2, "GetSpecular");
    lua_pushcfunction(m_L, Lua_MaterialSetSpecular);
    lua_setfield(m_L, -2, "SetSpecular");
    lua_pushcfunction(m_L, Lua_MaterialGetShininess);
    lua_setfield(m_L, -2, "GetShininess");
    lua_pushcfunction(m_L, Lua_MaterialSetShininess);
    lua_setfield(m_L, -2, "SetShininess");
    lua_pushcfunction(m_L, Lua_MaterialGetTexture);
    lua_setfield(m_L, -2, "GetTexture");
    lua_pushcfunction(m_L, Lua_MaterialSetTexture);
    lua_setfield(m_L, -2, "SetTexture");
    lua_pushcfunction(m_L, Lua_MaterialUseTexture);
    lua_setfield(m_L, -2, "UseTexture");
    lua_pushcfunction(m_L, Lua_MaterialSetUseTexture);
    lua_setfield(m_L, -2, "SetUseTexture");
    lua_setglobal(m_L, "Material");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_LightGetDirection);
    lua_setfield(m_L, -2, "GetDirection");
    lua_pushcfunction(m_L, Lua_LightSetDirection);
    lua_setfield(m_L, -2, "SetDirection");
    lua_pushcfunction(m_L, Lua_LightGetColor);
    lua_setfield(m_L, -2, "GetColor");
    lua_pushcfunction(m_L, Lua_LightSetColor);
    lua_setfield(m_L, -2, "SetColor");
    lua_pushcfunction(m_L, Lua_LightGetIntensity);
    lua_setfield(m_L, -2, "GetIntensity");
    lua_pushcfunction(m_L, Lua_LightSetIntensity);
    lua_setfield(m_L, -2, "SetIntensity");
    lua_setglobal(m_L, "Light");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_AudioPlay);
    lua_setfield(m_L, -2, "Play");
    lua_pushcfunction(m_L, Lua_AudioStop);
    lua_setfield(m_L, -2, "Stop");
    lua_pushcfunction(m_L, Lua_AudioPlayOneShot);
    lua_setfield(m_L, -2, "PlayOneShot");
    lua_setglobal(m_L, "Audio");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_SceneLoadByBuildIndex);
    lua_setfield(m_L, -2, "LoadByBuildIndex");
    lua_pushcfunction(m_L, Lua_SceneLoadByName);
    lua_setfield(m_L, -2, "LoadByName");
    lua_setglobal(m_L, "Scene");

    lua_newtable(m_L);
    lua_pushcfunction(m_L, Lua_DialogueGetEntryCount);
    lua_setfield(m_L, -2, "GetEntryCount");
    lua_pushcfunction(m_L, Lua_DialogueGetEntryByIndex);
    lua_setfield(m_L, -2, "GetEntryByIndex");
    lua_pushcfunction(m_L, Lua_DialogueGetEntryText);
    lua_setfield(m_L, -2, "GetEntryText");
    lua_setglobal(m_L, "Dialogue");

    EventBus::Subscribe("UIButtonClicked", [this](const EngineEvent& event)
        {
            if (!event.payload.empty())
            {
                const Entity entity{ event.entityId };
                if (CallNamedFunction(event.payload, entity))
                {
                    EditorConsole::Log("[Lua] UI callback '" + event.payload + "' invoked on entity " + std::to_string(entity.id) + ".");
                }
                else
                {
                    EditorConsole::Warn("[Lua] UI callback '" + event.payload + "' could not be invoked on entity " + std::to_string(entity.id) + ".");
                }
            }
            else
            {
                EditorConsole::Warn("[Lua] UI button click ignored because no function is bound.");
            }
        });
}

bool ScriptSystem::CallNamedFunction(const std::string& functionName, const Entity entity)
{
    if (m_L == nullptr || functionName.empty())
    {
        return false;
    }

    if (entityManager == nullptr || !entityManager->IsAlive(entity))
    {
        EditorConsole::Error("[Lua] UI callback ignored because the target entity is invalid.");
        return false;
    }

    if (components == nullptr || !components->HasComponent<ScriptComponent>(entity))
    {
        EditorConsole::Error("[Lua] UI callback ignored because the button entity has no Script component.");
        return false;
    }

    const auto& script = components->GetComponent<ScriptComponent>(entity);
    if (script.ScriptPath.empty())
    {
        EditorConsole::Error("[Lua] UI callback ignored because the button Script component has no script path.");
        return false;
    }

    lua_pushcfunction(m_L, LuaTraceback);
    const int errFunc = lua_gettop(m_L);

    lua_getglobal(m_L, functionName.c_str());
    if (!lua_isfunction(m_L, -1))
    {
        EditorConsole::Warn("[Lua] Function '" + functionName + "' is not available in the current Lua state.");
        lua_pop(m_L, 1);
        lua_pop(m_L, 1);
        return false;
    }

    int argCount = 1;
    PushEntity(m_L, entity);

    if (components != nullptr && components->HasComponent<RuntimeUIComponent>(entity))
    {
        const auto& ui = components->GetComponent<RuntimeUIComponent>(entity);
        switch (ui.luaArgumentType)
        {
        case RuntimeUIArgumentType::String:
            lua_pushstring(m_L, ui.luaStringArgument.c_str());
            ++argCount;
            break;
        case RuntimeUIArgumentType::Bool:
            lua_pushboolean(m_L, ui.luaBoolArgument ? 1 : 0);
            ++argCount;
            break;
        case RuntimeUIArgumentType::Int:
            lua_pushinteger(m_L, ui.luaIntArgument);
            ++argCount;
            break;
        case RuntimeUIArgumentType::None:
        default:
            break;
        }
    }

    if (lua_pcall(m_L, argCount, 0, errFunc) != LUA_OK)
    {
        const char* err = lua_tostring(m_L, -1);
        EditorConsole::Error(err != nullptr ? err : "[Lua] UI callback failed.");
        lua_pop(m_L, 1);
        lua_remove(m_L, errFunc);
        return false;
    }

    lua_remove(m_L, errFunc);
    return true;
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
        m_LastReloadSucceeded = false;
        m_LastReloadMessage = "[Lua] Failed to load script: " + script.ScriptPath;

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
    script.OnTriggerEnter = GetFunctionRef("OnTriggerEnter");
    script.OnTriggerStay = GetFunctionRef("OnTriggerStay");
    script.OnTriggerExit = GetFunctionRef("OnTriggerExit");
    script.Started = false;
    m_LastWriteTimes[script.ScriptPath] = std::filesystem::last_write_time(fullPath);
    m_LastReloadSucceeded = true;
    m_LastReloadMessage = "[Lua] Loaded script: " + script.ScriptPath;

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

void ScriptSystem::CallTriggerFunction(int fnRef, Entity entity, Entity other)
{
    if (fnRef == LUA_REFNIL)
        return;

    lua_pushcfunction(m_L, LuaTraceback);
    const int errFunc = lua_gettop(m_L);

    lua_rawgeti(m_L, LUA_REGISTRYINDEX, fnRef);
    PushEntity(m_L, entity);
    PushEntity(m_L, other);

    if (lua_pcall(m_L, 2, 0, errFunc) != LUA_OK)
    {
        const char* err = lua_tostring(m_L, -1);
        const std::string errStr = err ? err : "[Lua] Runtime trigger error";

        if (auto parsed = ParseLuaError(errStr, "<runtime>", true))
        {
            EditorConsole::Error(errStr, parsed->scriptPath, parsed->line);
            m_Errors.push_back(*parsed);
        }
        else
        {
            EditorConsole::Error(errStr);
        }

        lua_pop(m_L, 1);
    }

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

    if (components == nullptr || !components->HasComponent<PhysicsComponent>(entity))
    {
        return;
    }

    auto& physics = components->GetComponent<PhysicsComponent>(entity);

    for (const EntityID otherId : physics.triggerEntities)
    {
        const Entity other{ otherId };
        const bool wasOverlapping = std::find(
            physics.previousTriggerEntities.begin(),
            physics.previousTriggerEntities.end(),
            otherId) != physics.previousTriggerEntities.end();

        if (!wasOverlapping)
        {
            CallTriggerFunction(script.OnTriggerEnter, entity, other);
        }

        CallTriggerFunction(script.OnTriggerStay, entity, other);
    }

    for (const EntityID otherId : physics.previousTriggerEntities)
    {
        const bool stillOverlapping = std::find(
            physics.triggerEntities.begin(),
            physics.triggerEntities.end(),
            otherId) != physics.triggerEntities.end();
        if (!stillOverlapping)
        {
            CallTriggerFunction(script.OnTriggerExit, entity, Entity{ otherId });
        }
    }

    physics.previousTriggerEntities = physics.triggerEntities;
}

void ScriptSystem::AutoReloadModifiedScripts(const EntityManager& entities, ComponentManager& components)
{
    if (!ProjectManager::HasActiveProject())
    {
        return;
    }

    const auto& project = ProjectManager::GetActive();
    for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) || !components.HasComponent<ScriptComponent>(entity))
        {
            continue;
        }

        auto& script = components.GetComponent<ScriptComponent>(entity);
        if (script.ScriptPath.empty())
        {
            continue;
        }

        const std::filesystem::path fullPath = project.rootPath / script.ScriptPath;
        if (!std::filesystem::exists(fullPath))
        {
            continue;
        }

        const auto writeTime = std::filesystem::last_write_time(fullPath);
        const auto existing = m_LastWriteTimes.find(script.ScriptPath);
        if (existing != m_LastWriteTimes.end() && existing->second >= writeTime)
        {
            continue;
        }

        std::ifstream file(fullPath);
        std::stringstream buffer;
        buffer << file.rdbuf();

        std::string error;
        if (!ValidateScriptText(buffer.str(), error))
        {
            m_LastReloadSucceeded = false;
            m_LastReloadMessage = "[Lua] Auto-reload failed for " + script.ScriptPath;
            if (auto parsed = ParseLuaError(error, script.ScriptPath, false))
            {
                EditorConsole::Error(
                    "[Lua] Auto-reload failed for " + script.ScriptPath + ": " + parsed->message,
                    parsed->scriptPath,
                    parsed->line);
                m_Errors.push_back(*parsed);
            }
            else
            {
                EditorConsole::Error("[Lua] Auto-reload failed for " + script.ScriptPath + ": " + error);
            }
            m_LastWriteTimes[script.ScriptPath] = writeTime;
            continue;
        }

        LoadScript(script);
        m_LastReloadSucceeded = true;
        m_LastReloadMessage = "[Lua] Auto-reloaded script: " + script.ScriptPath;
        EditorConsole::Log(m_LastReloadMessage);
    }
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

static int Lua_GetPosition(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (const TransformComponent* transform = GetTransform(entity))
    {
        PushVec3(L, transform->position.x, transform->position.y, transform->position.z);
        return 3;
    }

    return 0;
}

static int Lua_SetPosition(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (TransformComponent* transform = GetTransform(entity))
    {
        transform->position.x = static_cast<float>(luaL_optnumber(L, 2, transform->position.x));
        transform->position.y = static_cast<float>(luaL_optnumber(L, 3, transform->position.y));
        transform->position.z = static_cast<float>(luaL_optnumber(L, 4, transform->position.z));
    }

    return 0;
}

static int Lua_GetRotation(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (const TransformComponent* transform = GetTransform(entity))
    {
        PushVec3(L, transform->rotation.x, transform->rotation.y, transform->rotation.z);
        return 3;
    }

    return 0;
}

static int Lua_SetRotation(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (TransformComponent* transform = GetTransform(entity))
    {
        transform->rotation.x = static_cast<float>(luaL_optnumber(L, 2, transform->rotation.x));
        transform->rotation.y = static_cast<float>(luaL_optnumber(L, 3, transform->rotation.y));
        transform->rotation.z = static_cast<float>(luaL_optnumber(L, 4, transform->rotation.z));
    }

    return 0;
}

static int Lua_Translate(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (TransformComponent* transform = GetTransform(entity))
    {
        transform->position.x += static_cast<float>(luaL_optnumber(L, 2, 0.0));
        transform->position.y += static_cast<float>(luaL_optnumber(L, 3, 0.0));
        transform->position.z += static_cast<float>(luaL_optnumber(L, 4, 0.0));
    }

    return 0;
}

static int Lua_Rotate(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (TransformComponent* transform = GetTransform(entity))
    {
        transform->rotation.x += static_cast<float>(luaL_optnumber(L, 2, 0.0));
        transform->rotation.y += static_cast<float>(luaL_optnumber(L, 3, 0.0));
        transform->rotation.z += static_cast<float>(luaL_optnumber(L, 4, 0.0));
    }

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

static int Lua_PhysicsSetEnabled(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* physics = GetPhysics(entity))
    {
        physics->enabled = lua_toboolean(L, 2) != 0;
    }

    return 0;
}

static int Lua_PhysicsIsEnabled(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    if (const auto* physics = GetPhysics(entity))
    {
        lua_pushboolean(L, physics->enabled);
        return 1;
    }

    lua_pushboolean(L, false);
    return 1;
}

static int Lua_PhysicsIsTouchingTag(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const char* tagName = luaL_optstring(L, 2, "");
    if (!valid || tagName == nullptr)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    if (const auto* physics = GetPhysics(entity))
    {
        const bool touching = std::find(
            physics->overlappingTags.begin(),
            physics->overlappingTags.end(),
            tagName) != physics->overlappingTags.end();
        lua_pushboolean(L, touching);
        return 1;
    }

    lua_pushboolean(L, false);
    return 1;
}

static int Lua_EntityDestroy(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    auto& scriptSystem = ScriptSystem::Get();
    if (auto* components = scriptSystem.GetComponents())
    {
        DestroyKnownComponents(*components, entity);
    }
    if (auto* entities = scriptSystem.GetEntities())
    {
        entities->DestroyEntity(entity);
    }

    return 0;
}

static int Lua_EntityHasComponent(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const char* componentName = luaL_optstring(L, 2, "");
    if (!valid || componentName == nullptr)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    auto* components = ScriptSystem::Get().GetComponents();
    lua_pushboolean(L, components != nullptr && HasNamedComponent(*components, entity, componentName));
    return 1;
}

static int Lua_EntitySpawn(lua_State* L)
{
    auto& scriptSystem = ScriptSystem::Get();
    auto* entities = scriptSystem.GetEntities();
    auto* components = scriptSystem.GetComponents();
    if (entities == nullptr || components == nullptr)
    {
        return 0;
    }

    const Entity entity = entities->CreateEntity();
    if (!components->HasComponent<TransformComponent>(entity))
    {
        components->AddComponent(entity, TransformComponent{});
    }
    if (!components->HasComponent<TagComponent>(entity))
    {
        components->AddComponent(entity, TagComponent{});
    }

    PushEntity(L, entity);
    return 1;
}

static int Lua_EntityGetTag(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* tag = valid ? GetTag(entity) : nullptr;
    lua_pushstring(L, tag != nullptr ? tag->tag.c_str() : "");
    return 1;
}

static int Lua_EntitySetTag(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    auto* components = ScriptSystem::Get().GetComponents();
    if (components == nullptr)
    {
        return 0;
    }

    if (!components->HasComponent<TagComponent>(entity))
    {
        components->AddComponent(entity, TagComponent{});
    }

    auto& tag = components->GetComponent<TagComponent>(entity);
    const char* value = luaL_optstring(L, 2, tag.tag.c_str());
    tag.tag = value != nullptr ? value : "";
    return 0;
}

static int Lua_MaterialGetAlbedo(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (const auto* material = GetMaterial(entity))
    {
        PushVec3(L, material->albedo.x, material->albedo.y, material->albedo.z);
        return 3;
    }

    return 0;
}

static int Lua_MaterialSetAlbedo(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* material = GetMaterial(entity))
    {
        material->albedo.x = static_cast<float>(luaL_optnumber(L, 2, material->albedo.x));
        material->albedo.y = static_cast<float>(luaL_optnumber(L, 3, material->albedo.y));
        material->albedo.z = static_cast<float>(luaL_optnumber(L, 4, material->albedo.z));
    }

    return 0;
}

static int Lua_MaterialGetSpecular(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* material = valid ? GetMaterial(entity) : nullptr;
    lua_pushnumber(L, material != nullptr ? material->specular : 0.0f);
    return 1;
}

static int Lua_MaterialSetSpecular(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* material = GetMaterial(entity))
    {
        material->specular = static_cast<float>(luaL_optnumber(L, 2, material->specular));
    }

    return 0;
}

static int Lua_MaterialGetShininess(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* material = valid ? GetMaterial(entity) : nullptr;
    lua_pushnumber(L, material != nullptr ? material->shininess : 0.0f);
    return 1;
}

static int Lua_MaterialSetShininess(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* material = GetMaterial(entity))
    {
        material->shininess = static_cast<float>(luaL_optnumber(L, 2, material->shininess));
    }

    return 0;
}

static int Lua_MaterialGetTexture(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* material = valid ? GetMaterial(entity) : nullptr;
    lua_pushstring(L, material != nullptr ? material->albedoTexture.c_str() : "");
    return 1;
}

static int Lua_MaterialSetTexture(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* material = GetMaterial(entity))
    {
        const char* path = luaL_optstring(L, 2, material->albedoTexture.c_str());
        material->albedoTexture = path != nullptr ? path : "";
        material->useTexture = !material->albedoTexture.empty();
    }

    return 0;
}

static int Lua_MaterialUseTexture(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* material = valid ? GetMaterial(entity) : nullptr;
    lua_pushboolean(L, material != nullptr && material->useTexture);
    return 1;
}

static int Lua_MaterialSetUseTexture(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* material = GetMaterial(entity))
    {
        material->useTexture = lua_toboolean(L, 2) != 0;
    }

    return 0;
}

static int Lua_LightGetDirection(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (const auto* light = GetLight(entity))
    {
        PushVec3(L, light->direction.x, light->direction.y, light->direction.z);
        return 3;
    }

    return 0;
}

static int Lua_LightSetDirection(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* light = GetLight(entity))
    {
        light->direction.x = static_cast<float>(luaL_optnumber(L, 2, light->direction.x));
        light->direction.y = static_cast<float>(luaL_optnumber(L, 3, light->direction.y));
        light->direction.z = static_cast<float>(luaL_optnumber(L, 4, light->direction.z));
    }

    return 0;
}

static int Lua_LightGetColor(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (const auto* light = GetLight(entity))
    {
        PushVec3(L, light->color.x, light->color.y, light->color.z);
        return 3;
    }

    return 0;
}

static int Lua_LightSetColor(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* light = GetLight(entity))
    {
        light->color.x = static_cast<float>(luaL_optnumber(L, 2, light->color.x));
        light->color.y = static_cast<float>(luaL_optnumber(L, 3, light->color.y));
        light->color.z = static_cast<float>(luaL_optnumber(L, 4, light->color.z));
    }

    return 0;
}

static int Lua_LightGetIntensity(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* light = valid ? GetLight(entity) : nullptr;
    lua_pushnumber(L, light != nullptr ? light->intensity : 0.0f);
    return 1;
}

static int Lua_LightSetIntensity(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    if (!valid)
    {
        return 0;
    }

    if (auto* light = GetLight(entity))
    {
        light->intensity = static_cast<float>(luaL_optnumber(L, 2, light->intensity));
    }

    return 0;
}

static int Lua_AudioPlay(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    auto& scriptSystem = ScriptSystem::Get();
    if (!valid || scriptSystem.GetAudioSystem() == nullptr)
    {
        return 0;
    }

    if (auto* source = GetAudioSource(entity))
    {
        scriptSystem.GetAudioSystem()->Play(entity, *source);
        source->started = true;
    }

    return 0;
}

static int Lua_AudioStop(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    auto& scriptSystem = ScriptSystem::Get();
    if (!valid || scriptSystem.GetAudioSystem() == nullptr)
    {
        return 0;
    }

    scriptSystem.GetAudioSystem()->Stop(entity);
    if (auto* source = GetAudioSource(entity))
    {
        source->playing = false;
    }

    return 0;
}

static int Lua_AudioPlayOneShot(lua_State* L)
{
    auto& scriptSystem = ScriptSystem::Get();
    if (scriptSystem.GetAudioSystem() == nullptr)
    {
        return 0;
    }

    const char* path = luaL_optstring(L, 1, "");
    if (path != nullptr && path[0] != '\0')
    {
        scriptSystem.GetAudioSystem()->PlayOneShot(path);
    }

    return 0;
}

static int Lua_SceneLoadByBuildIndex(lua_State* L)
{
    const int buildIndex = static_cast<int>(luaL_optinteger(L, 1, -1));
    if (buildIndex >= 0)
    {
        EventBus::Publish({ "LoadSceneByBuildIndex", 0, {}, std::to_string(buildIndex) });
    }

    return 0;
}

static int Lua_SceneLoadByName(lua_State* L)
{
    const char* sceneName = luaL_optstring(L, 1, "");
    if (sceneName != nullptr && sceneName[0] != '\0')
    {
        EventBus::Publish({ "LoadSceneByName", 0, sceneName, sceneName });
    }

    return 0;
}

static int Lua_DialogueGetEntryCount(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const auto* dialogue = valid ? GetDialogue(entity) : nullptr;
    lua_pushinteger(L, dialogue != nullptr ? static_cast<lua_Integer>(dialogue->entries.size()) : 0);
    return 1;
}

static int Lua_DialogueGetEntryByIndex(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const int index = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (!valid || index < 0)
    {
        return 0;
    }

    const auto* dialogue = GetDialogue(entity);
    if (dialogue == nullptr || index >= static_cast<int>(dialogue->entries.size()))
    {
        return 0;
    }

    const auto& entry = dialogue->entries[static_cast<std::size_t>(index)];
    lua_pushinteger(L, entry.id);
    lua_pushstring(L, entry.text.c_str());
    return 2;
}

static int Lua_DialogueGetEntryText(lua_State* L)
{
    bool valid = false;
    const Entity entity = GetEntityArg(L, 1, &valid);
    const int entryId = static_cast<int>(luaL_optinteger(L, 2, -1));
    if (!valid || entryId < 0)
    {
        lua_pushstring(L, "");
        return 1;
    }

    const auto* dialogue = GetDialogue(entity);
    const auto* entry = dialogue != nullptr ? dialogue->FindEntryById(entryId) : nullptr;
    lua_pushstring(L, entry != nullptr ? entry->text.c_str() : "");
    return 1;
}


void ScriptSystem::SetInputSystem(InputSystem* input)
{
    m_InputSystem = input;
}

void ScriptSystem::SetAudioSystem(AudioSystem* audioSystem)
{
    m_AudioSystem = audioSystem;
}


