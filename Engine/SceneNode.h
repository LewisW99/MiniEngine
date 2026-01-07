#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cmath>

#include "../Engine/Math/MathTypes.h"  // (Vec3, Mat4, etc.)

// ------------------------------------------------------------
// SceneNode
// ------------------------------------------------------------
struct SceneNode {
    std::string name;
    Vec3 position{ 0,0,0 };
    Vec3 rotation{ 0,0,0 };   // Euler angles for now
    Vec3 scale{ 1,1,1 };

    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;

    Mat4 localMatrix;   // from position/rotation/scale
    Mat4 worldMatrix;   // full transform

    // Optional local-space AABB (used later for culling)
    AABB localBounds;
    AABB worldBounds;

    SceneNode(const std::string& nodeName = "Node");

    void AddChild(std::unique_ptr<SceneNode> child);
    void UpdateTransform(const Mat4& parentMatrix = Mat4::Identity());
};


