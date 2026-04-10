#pragma once

#include <string>

struct AudioSourceComponent
{
    std::string path;
    bool loop = false;
    bool playOnStart = true;
    float volume = 1.0f;
    bool spatial = false;
    bool enabled = true;
    bool playing = false;
    bool started = false;
};
