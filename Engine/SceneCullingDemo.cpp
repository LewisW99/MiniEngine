#include "pch.h"
#include "SceneCullingDemo.h"

//Test scene with some nodes
void SceneCullingDemo::BuildScene() {
    // create a few nodes with random positions
    for (int i = 0; i < 8; ++i) {
        auto node = std::make_unique<SceneNode>("Node_" + std::to_string(i));
        node->position = { float(i * 2 - 7), 0, float(i * 2 - 7) };  // spread them out
        node->localBounds = { {-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f} };
        allNodes.push_back(node.get());
        root.AddChild(std::move(node));
    }
    root.UpdateTransform(Mat4::Identity());
}

//parallel culling
void SceneCullingDemo::CullVisible(const Frustum& frustum, JobSystem& jobSystem) {
    Job* rootJob = jobSystem.CreateJob([]() {});

    for (SceneNode* node : allNodes) {
        Job* cullJob = jobSystem.CreateJob([node, &frustum]() {
            if (FrustumCuller::IsVisible(node->worldBounds, frustum))
                std::cout << "[Culling] " << node->name << " visible\n";
            else
                std::cout << "[Culling] " << node->name << " culled\n";
            }, rootJob);
        jobSystem.Run(cullJob);
    }

    jobSystem.Run(rootJob);
    jobSystem.Wait(rootJob);
}

