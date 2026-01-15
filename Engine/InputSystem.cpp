#include "pch.h"
#include "InputSystem.h"

void InputSystem::Init() {}
void InputSystem::Shutdown() {}

static bool s_GameplayEnabled = true;

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
}

void InputSystem::EndFrame() {}

void InputSystem::OnKeyDown(int scancode)
{
    if (!s_GameplayEnabled)
    {
        return;
    }

    for (auto& [name, action] : m_Actions)
    {
        if (action.key == scancode && !action.down)
        {
            action.down = true;
            action.state = InputActionState::Pressed;
        }
    }
}

void InputSystem::OnKeyUp(int scancode)
{
    if (!s_GameplayEnabled)
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



void InputSystem::SetGameplayEnabled(bool enabled)
{
    s_GameplayEnabled = enabled;
}
