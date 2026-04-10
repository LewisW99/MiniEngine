#pragma once
#include "../ECS/Entity.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <filesystem>

struct ScriptComponent;

struct ScriptError
{
    std::string scriptPath;
    int line = -1;
    std::string message;
    bool runtime = false;
};

class ComponentManager;
class EntityManager;
class InputSystem;
class AudioSystem;

class ScriptSystem
{
public:
    void Init(ComponentManager* components, EntityManager* entities);
    void Shutdown();

    void LoadScript(ScriptComponent& script);

    ComponentManager* GetComponents() const { return components; }
    EntityManager* GetEntities() const { return entityManager; }
    AudioSystem* GetAudioSystem() const { return m_AudioSystem; }
    static ScriptSystem& Get();
    void Update(Entity entity, ScriptComponent& script, float dt);
	bool ValidateScriptText(const std::string& scriptText, std::string& errorOut);
    void AutoReloadModifiedScripts(const EntityManager& entities, ComponentManager& components);

    const std::vector<ScriptError>& GetErrors() const { return m_Errors; }
    void ClearErrors() { m_Errors.clear(); }
    const std::string& GetLastReloadMessage() const { return m_LastReloadMessage; }
    bool WasLastReloadSuccessful() const { return m_LastReloadSucceeded; }


    void SetInputSystem(InputSystem* input);
    void SetAudioSystem(AudioSystem* audioSystem);
    InputSystem* m_InputSystem = nullptr;
    AudioSystem* m_AudioSystem = nullptr;

private:
    struct lua_State* m_L = nullptr;

    int GetFunctionRef(const char* name);
    void CallFunction(int fnRef, Entity entity, float dt);
    ComponentManager* components = nullptr;
    EntityManager* entityManager = nullptr;
    
    std::vector<ScriptError> m_Errors;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_LastWriteTimes;
    std::string m_LastReloadMessage;
    bool m_LastReloadSucceeded = true;

};
