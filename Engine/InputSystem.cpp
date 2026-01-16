#include "pch.h"
#include "InputSystem.h"
#include <glm/common.hpp>

void InputSystem::Init() {}
void InputSystem::Shutdown() {}


void InputSystem::BindAction(const std::string& action, int scancode)
{
    InputAction& a = m_Actions[action];
    a.name = action;
    a.key = scancode;
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