#pragma once
#include <string>

struct ScriptComponent
{
    std::string ScriptPath;

    int OnStart = -1;
    int OnUpdate = -1;
    int OnDestroy = -1;

    bool Started = false;
};
