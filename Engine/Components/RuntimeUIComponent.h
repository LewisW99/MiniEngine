#pragma once

#include <string>

enum class RuntimeUIAnchor
{
    TopLeft = 0,
    TopCenter,
    TopRight,
    Center,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class RuntimeUIElementType
{
    Text = 0,
    Button,
    Image
};

struct RuntimeUIComponent
{
    RuntimeUIElementType type = RuntimeUIElementType::Text;
    RuntimeUIAnchor anchor = RuntimeUIAnchor::TopLeft;
    std::string text{ "UI" };
    std::string texturePath{};
    std::string buttonEvent{ "UIButtonClicked" };
    float offsetX = 10.0f;
    float offsetY = 10.0f;
    float width = 220.0f;
    float height = 48.0f;
    bool visible = true;
};
