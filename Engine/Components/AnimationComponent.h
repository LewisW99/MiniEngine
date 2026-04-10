#pragma once

#include <string>
#include <vector>
#include "../Math/MathTypes.h"

struct TransformKeyframe
{
    float time = 0.0f;
    Vec3 value{};
};

struct TransformAnimationClip
{
    std::string name{ "Default" };
    std::vector<TransformKeyframe> positionKeys;
    std::vector<TransformKeyframe> rotationKeys;
    std::vector<TransformKeyframe> scaleKeys;
    float duration = 0.0f;
};

struct AnimationComponent
{
    TransformAnimationClip clip{};
    bool playing = false;
    bool loop = true;
    float currentTime = 0.0f;
};
