#include "pch.h"
#include "InputSystem.h"
#include <filesystem>
#include <fstream>
#include <glm/common.hpp>
#include <nlohmann/json.hpp>

void InputSystem::Init() {}
void InputSystem::Shutdown() {}


void InputSystem::BindAction(const std::string& action, int scancode)
{
    InputAction& a = m_Actions[action];
    a.name = action;
    a.key = scancode;
}

void InputSystem::RebindAction(const std::string& action, const int scancode)
{
    BindAction(action, scancode);
}

void InputSystem::BeginFrame()
{
    m_MouseDX = 0.0f;
	m_MouseDY = 0.0f;


    for (auto& [name, action] : m_Actions)
    {
        if (action.state == InputActionState::Pressed)
            action.state = InputActionState::Held;
        else if (action.state == InputActionState::Released)
            action.state = InputActionState::None;
    }

    for (auto& [axisName, axis] : m_Axes)
    {
        float value = 0.0f;

        for (const auto& binding : axis.bindings)
        {
            if (Held(binding.positive))
                value += 1.0f;

            if (Held(binding.negative))
                value -= 1.0f;
        }

        axis.value = glm::clamp(value, -1.0f, 1.0f);
    }
}

void InputSystem::EndFrame() {}

void InputSystem::OnKeyDown(int scancode)
{
    if (!m_GameplayEnabled)
        return;

    for (auto& [name, action] : m_Actions)
    {
        if (action.key == scancode)
        {
            if (!action.down)
            {
                action.down = true;
                action.state = InputActionState::Pressed;
            }
        }
    }
}

void InputSystem::BindAxis(
    const std::string& axis,
    const std::string& positiveAction,
    const std::string& negativeAction
)
{
    InputAxis& a = m_Axes[axis];
    a.name = axis;
    a.bindings.push_back({ positiveAction, negativeAction });
}

float InputSystem::GetAxis(const std::string& axis) const
{
    auto it = m_Axes.find(axis);
    return it != m_Axes.end() ? it->second.value : 0.0f;
}


void InputSystem::OnKeyUp(int scancode)
{
    if (!m_GameplayEnabled)
    {
        return;
    }

    for (auto& [name, action] : m_Actions)
    {
        if (action.key == scancode)
        {
            action.down = false;
            action.state = InputActionState::Released;
        }
    }
}

bool InputSystem::Pressed(const std::string& action) const
{
    auto it = m_Actions.find(action);
    return it != m_Actions.end() &&
        it->second.state == InputActionState::Pressed;
}

bool InputSystem::Held(const std::string& action) const
{
    auto it = m_Actions.find(action);
    return it != m_Actions.end() &&
        it->second.state == InputActionState::Held;
}

bool InputSystem::Released(const std::string& action) const
{
    auto it = m_Actions.find(action);
    return it != m_Actions.end() &&
        it->second.state == InputActionState::Released;
}

void InputSystem::OnMouseMove(float dx, float dy)
{
    m_MouseDX += dx;
    m_MouseDY += dy;
}

float InputSystem::GetMouseDX() const
{
    return m_MouseDX;
}

float InputSystem::GetMouseDY() const
{
    return m_MouseDY;
}

const std::unordered_map<std::string, InputAction>& InputSystem::GetActions() const
{
	return m_Actions;
}

void InputSystem::SetGameplayEnabled(bool enabled)
{
    m_GameplayEnabled = enabled;
}

bool InputSystem::IsGameplayEnabled() const
{
    return m_GameplayEnabled;
}

bool InputSystem::SaveBindings(const std::string& path) const
{
    nlohmann::json root;
    root["actions"] = nlohmann::json::array();
    root["axes"] = nlohmann::json::array();

    for (const auto& [name, action] : m_Actions)
    {
        root["actions"].push_back({
            { "name", name },
            { "key", action.key }
        });
    }

    for (const auto& [name, axis] : m_Axes)
    {
        nlohmann::json axisEntry = {
            { "name", name },
            { "bindings", nlohmann::json::array() }
        };

        for (const auto& binding : axis.bindings)
        {
            axisEntry["bindings"].push_back({
                { "positive", binding.positive },
                { "negative", binding.negative }
            });
        }

        root["axes"].push_back(axisEntry);
    }

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    file << root.dump(4);
    return true;
}

bool InputSystem::LoadBindings(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json root;
    file >> root;

    if (root.contains("actions"))
    {
        for (const auto& action : root["actions"])
        {
            BindAction(action.value("name", ""), action.value("key", 0));
        }
    }

    if (root.contains("axes"))
    {
        m_Axes.clear();
        for (const auto& axis : root["axes"])
        {
            const std::string name = axis.value("name", "");
            for (const auto& binding : axis["bindings"])
            {
                BindAxis(name, binding.value("positive", ""), binding.value("negative", ""));
            }
        }
    }

    return true;
}
