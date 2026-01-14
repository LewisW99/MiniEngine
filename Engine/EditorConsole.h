#pragma once
#include <string>
#include <vector>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

struct ConsoleMessage
{
    LogLevel level;
    std::string text;

    bool hasScriptLocation = false;
    std::string scriptPath;
    int line = -1;
};

class EditorConsole
{
public:
    static void Log(const std::string& msg);
    static void Warn(const std::string& msg);
    static void Error(const std::string& msg);

    static const std::vector<ConsoleMessage>& GetMessages();
    static void Clear();

    static void Error(
        const std::string& text,
        const std::string& scriptPath,
        int line);

private:
    static std::vector<ConsoleMessage> s_Messages;
};
