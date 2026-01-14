#pragma once
#include <string>
#include "InputTypes.h"

struct InputAction
{
    std::string name;
    int key = -1;                 // SDL_Scancode
    InputActionState state = InputActionState::None;
    bool down = false;
};
