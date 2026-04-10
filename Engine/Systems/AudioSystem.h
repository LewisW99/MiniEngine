#pragma once

#include <string>
#include <unordered_map>
#include <miniaudio.h>
#include "../Components/AudioSourceComponent.h"
#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"

class AudioSystem
{
public:
    ~AudioSystem();

    bool Init();
    void Shutdown();
    void Update(const EntityManager& entities, ComponentManager& components);
    void Play(Entity entity, AudioSourceComponent& source);
    void Stop(Entity entity);
    void PlayOneShot(const std::string& path);

private:
    struct SoundState
    {
        ma_sound sound{};
        std::string loadedPath;
        bool initialized = false;
    };

    std::unordered_map<EntityID, SoundState> m_Sounds;
    bool EnsureSoundLoaded(Entity entity, const std::string& path);
    void ReleaseSound(Entity entity);
};
