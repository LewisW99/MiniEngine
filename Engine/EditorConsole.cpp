#include "pch.h"
#include "EditorConsole.h"

std::vector<ConsoleMessage> EditorConsole::s_Messages;

void EditorConsole::Log(const std::string& msg)
{
    s_Messages.push_back({ LogLevel::Info, msg });
}

void EditorConsole::Warn(const std::string& msg)
{
    s_Messages.push_back({ LogLevel::Warning, msg });
}

void EditorConsole::Error(const std::string& msg)
{
    s_Messages.push_back({ LogLevel::Error, msg });
}

const std::vector<ConsoleMessage>& EditorConsole::GetMessages()
{
    return s_Messages;
}

void EditorConsole::Clear()
{
    s_Messages.clear();
}

void EditorConsole::Error(
    const std::string& text,
    const std::string& scriptPath,
    int line)
{
    ConsoleMessage msg;
    msg.text = text;
    msg.level = LogLevel::Error;

    msg.hasScriptLocation = true;
    msg.scriptPath = scriptPath;
    msg.line = line;

    s_Messages.push_back(msg);
}