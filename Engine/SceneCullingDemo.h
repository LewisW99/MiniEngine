#pragma once
#include "../Engine/SceneNode.h"
#include "../Engine/FrustumCuller.h"
#include "../Engine/JobSystem.h"
#include <iostream>

// ------------------------------------------------------------
// SceneCullingDemo - builds a simple scene & runs parallel culling
// ------------------------------------------------------------
class SceneCullingDemo {
public:
    SceneNode root{ "Root" };
    std::vector<SceneNode*> allNodes;

    void BuildScene();
    void CullVisible(const Frustum& frustum, JobSystem& jobSystem);
};
