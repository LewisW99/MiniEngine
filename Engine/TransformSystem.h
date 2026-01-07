#pragma once
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/JobSystem.h"
#include "../Engine/Math/MathTypes.h"

struct TransformComponent {
    Vec3 position{ 0,0,0 };
    Vec3 rotation{ 0,0,0 };
    Vec3 scale{ 1,1,1 };
    Mat4 worldMatrix = Mat4::Identity();
};

//Parallel update of transforms
class TransformSystem {
public:
    static void Update(ComponentManager& cm, JobSystem& js) {
        auto& transforms = cm.GetAll<TransformComponent>();

        Job* root = js.CreateJob([]() {});
        for (size_t i = 0; i < transforms.size(); ++i) {
            TransformComponent* t = &transforms[i];
            Job* job = js.CreateJob([t]() {
                t->worldMatrix = Mat4::FromTRS(t->position, t->rotation, t->scale);
                }, root);
            js.Run(job);
        }
        js.Run(root);
        js.Wait(root);
    }
};
