#pragma once
#include <cstdint>

enum class InputActionState : uint8_t
{
    None = 0,
    Pressed,
    Held,
    Released
};

enum class MouseButton : uint8_t
{
    Left = 0,
    Right,
    Middle
};
