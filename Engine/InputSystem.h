#pragma once

#include <unordered_map>
#include <string>
#include "Input/InputAction.h"

struct InputAxisBinding
{
    std::string positive;
    std::string negative;
};

struct InputAxis
{
    std::string name;
    float value = 0.0f;
    std::vector<InputAxisBinding> bindings;
};
class InputSystem
{
public:
    void Init();
    void Shutdown();

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();

    // SDL events
    void OnKeyDown(int scancode);
    void OnKeyUp(int scancode);

    // Action binding
    void BindAction(const std::string& action, int scancode);

    // Queries
    bool Pressed(const std::string& action) const;
    bool Held(const std::string& action) const;
    bool Released(const std::string& action) const;
    void SetGameplayEnabled(bool enabled);

	bool IsGameplayEnabled() const;
	// Mouse movement
    void OnMouseMove(float dx, float dy);

    float GetMouseDX() const;
    float GetMouseDY() const;

    const std::unordered_map<std::string, InputAction>& GetActions() const;

    void BindAxis(
        const std::string& axis,
        const std::string& positiveAction,
        const std::string& negativeAction
    );
	float GetAxis(const std::string& axis) const;
private:
    std::unordered_map<std::string, InputAction> m_Actions;
    float m_MouseDX = 0.0f;
    float m_MouseDY = 0.0f;
	bool m_GameplayEnabled = true;

	std::unordered_map<std::string, InputAxis> m_Axes;
};
