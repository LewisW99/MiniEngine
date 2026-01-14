#pragma once
#include <string>
#include <vector>

struct ScriptApiFunction
{
    std::string name;
    std::string signature;
    std::string description;
};

struct ScriptApiCategory
{
    std::string name;
	std::string description;
    std::vector<ScriptApiFunction> functions;
};

// Global read-only registry
const std::vector<ScriptApiCategory>& GetScriptAPI();
