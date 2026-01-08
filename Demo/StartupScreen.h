#pragma once
#include <string>
#include <filesystem>

struct StartupResult
{
    bool projectChosen = false;
    std::filesystem::path projectPath;

    bool loadFailed = false;
};

class StartupScreen
{
public:
    StartupResult Draw();
    void NotifyLoadError();

private:
    bool bShowLoadError = false;
};