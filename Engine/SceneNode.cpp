#include "pch.h"
#include "SceneNode.h"
#include <cmath>

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
SceneNode::SceneNode(const std::string& nodeName)
    : name(nodeName) {
}

// ------------------------------------------------------------
// AddChild
// ------------------------------------------------------------
void SceneNode::AddChild(std::unique_ptr<SceneNode> child) {
    child->parent = this;
    children.emplace_back(std::move(child));
}

// ------------------------------------------------------------
// UpdateTransform (recursive)
// ------------------------------------------------------------
void SceneNode::UpdateTransform(const Mat4& parentMatrix) {
    // Compute local transform
    localMatrix = Mat4::FromTRS(position, rotation, scale);

    // Combine with parent
    worldMatrix = parentMatrix * localMatrix;

    // Update world-space bounds
    worldBounds = localBounds.Transform(worldMatrix);

    // Propagate down
    for (auto& child : children)
        child->UpdateTransform(worldMatrix);
}

