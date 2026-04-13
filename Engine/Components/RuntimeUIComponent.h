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
    Canvas = 0,
    Text,
    Button,
    Image
};

enum class RuntimeUIArgumentType
{
    None = 0,
    String,
    Bool,
    Int
};

struct RuntimeUIColor
{
    float r = 0.2f;
    float g = 0.2f;
    float b = 0.2f;
    float a = 1.0f;
};

struct RuntimeUIComponent
{
    bool isScreenUI = true;
    bool isCanvasRoot = false;
    RuntimeUIElementType type = RuntimeUIElementType::Text;
    RuntimeUIAnchor anchor = RuntimeUIAnchor::TopLeft;
    std::string canvasId{ "Canvas" };
    std::string text{ "UI" };
    std::string texturePath{};
    std::string buttonEvent{ "UIButtonClicked" };
    std::string luaFunction{};
    RuntimeUIArgumentType luaArgumentType = RuntimeUIArgumentType::None;
    std::string luaStringArgument{};
    bool luaBoolArgument = false;
    int luaIntArgument = 0;
    float offsetX = 10.0f;
    float offsetY = 10.0f;
    float width = 220.0f;
    float height = 48.0f;
    RuntimeUIColor buttonColor{ 0.24f, 0.44f, 0.76f, 1.0f };
    RuntimeUIColor buttonHoveredColor{ 0.30f, 0.52f, 0.86f, 1.0f };
    RuntimeUIColor buttonPressedColor{ 0.16f, 0.28f, 0.48f, 1.0f };
    RuntimeUIColor textColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    bool visible = true;
};
