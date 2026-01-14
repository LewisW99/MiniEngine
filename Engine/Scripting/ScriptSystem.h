#pragma once
#include "../ECS/Entity.h"
#include <string>
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
class ScriptSystem
{
public:
    void Init(ComponentManager* components);
    void Shutdown();

    void LoadScript(ScriptComponent& script);

    ComponentManager* GetComponents() const { return components; }
    static ScriptSystem& Get();
    void Update(Entity entity, ScriptComponent& script, float dt);
	bool ValidateScriptText(const std::string& scriptText, std::string& errorOut);

    const std::vector<ScriptError>& GetErrors() const { return m_Errors; }
    void ClearErrors() { m_Errors.clear(); }
private:
    struct lua_State* m_L = nullptr;

    int GetFunctionRef(const char* name);
    void CallFunction(int fnRef, Entity entity, float dt);
    ComponentManager* components = nullptr;
    
    std::vector<ScriptError> m_Errors;
};
