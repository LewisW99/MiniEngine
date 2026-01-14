#pragma once
#include "../ECS/Entity.h"

struct ScriptComponent;

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
private:
    struct lua_State* m_L = nullptr;

    int GetFunctionRef(const char* name);
    void CallFunction(int fnRef, Entity entity, float dt);
    ComponentManager* components = nullptr;
    
};
