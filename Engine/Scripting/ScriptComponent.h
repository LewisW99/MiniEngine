#pragma once
#include <string>

struct ScriptComponent
{
    std::string ScriptPath;

    int OnStart = -1;
    int OnUpdate = -1;
    int OnDestroy = -1;
    int OnTriggerEnter = -1;
    int OnTriggerStay = -1;
    int OnTriggerExit = -1;

    bool Started = false;
};
