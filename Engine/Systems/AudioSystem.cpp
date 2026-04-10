#include "pch.h"
#include "AudioSystem.h"

#include <filesystem>
#include <iostream>
#include "../AssetDatabase/AssetImporter.h"
#include "../../Demo/Editor/Managers/ProjectManager.h"

namespace
{
    std::string ResolveAudioPath(const std::string& path)
    {
        if (path.empty())
        {
            return {};
        }

        const std::filesystem::path source(path);
        if (source.is_absolute() || !ProjectManager::HasActiveProject())
        {
            return source.string();
        }

        return (ProjectManager::GetActive().rootPath / source).string();
    }
}

AudioSystem::~AudioSystem()
{
    Shutdown();
}

bool AudioSystem::Init()
{
    return AssetImporter::EnsureAudioEngine();
}

void AudioSystem::Shutdown()
{
    for (auto& [entityId, state] : m_Sounds)
    {
        if (state.initialized)
        {
            ma_sound_uninit(&state.sound);
        }
    }

    m_Sounds.clear();
}

void AudioSystem::Update(const EntityManager& entities, ComponentManager& components)
{
    for (auto it = m_Sounds.begin(); it != m_Sounds.end(); )
    {
        const Entity entity{ it->first };
        if (!entities.IsAlive(entity) || !components.HasComponent<AudioSourceComponent>(entity))
        {
            if (it->second.initialized)
            {
                ma_sound_uninit(&it->second.sound);
            }
            it = m_Sounds.erase(it);
            continue;
        }

        ++it;
    }

    for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) || !components.HasComponent<AudioSourceComponent>(entity))
        {
            continue;
        }

        auto& source = components.GetComponent<AudioSourceComponent>(entity);
        if (!source.enabled || source.path.empty())
        {
            Stop(entity);
            source.started = false;
            continue;
        }

        if (!EnsureSoundLoaded(entity, source.path))
        {
            continue;
        }

        auto& state = m_Sounds[entity.id];
        ma_sound_set_looping(&state.sound, source.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&state.sound, source.volume);

        if (source.playOnStart && !source.started)
        {
            Play(entity, source);
            source.started = true;
        }

        source.playing = state.initialized && ma_sound_is_playing(&state.sound) == MA_TRUE;
    }
}

void AudioSystem::Play(Entity entity, AudioSourceComponent& source)
{
    if (!EnsureSoundLoaded(entity, source.path))
    {
        return;
    }

    auto& state = m_Sounds[entity.id];
    ma_sound_set_looping(&state.sound, source.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&state.sound, source.volume);
    ma_sound_seek_to_pcm_frame(&state.sound, 0);
    ma_sound_start(&state.sound);
    source.playing = true;
}

void AudioSystem::Stop(Entity entity)
{
    if (const auto it = m_Sounds.find(entity.id); it != m_Sounds.end() && it->second.initialized)
    {
        ma_sound_stop(&it->second.sound);
    }
}

void AudioSystem::PlayOneShot(const std::string& path)
{
    if (!AssetImporter::EnsureAudioEngine())
    {
        return;
    }

    const std::string resolvedPath = ResolveAudioPath(path);
    if (resolvedPath.empty())
    {
        return;
    }

    if (ma_engine* engine = AssetImporter::GetAudioEngine())
    {
        if (ma_engine_play_sound(engine, resolvedPath.c_str(), nullptr) != MA_SUCCESS)
        {
            std::cerr << "[AudioSystem] Failed to play one-shot audio: " << resolvedPath << "\n";
        }
    }
}

bool AudioSystem::EnsureSoundLoaded(Entity entity, const std::string& path)
{
    if (!AssetImporter::EnsureAudioEngine())
    {
        return false;
    }

    const std::string resolvedPath = ResolveAudioPath(path);
    if (resolvedPath.empty())
    {
        return false;
    }

    auto& state = m_Sounds[entity.id];
    if (state.initialized && state.loadedPath == resolvedPath)
    {
        return true;
    }

    if (state.initialized)
    {
        ma_sound_uninit(&state.sound);
        state.initialized = false;
        state.loadedPath.clear();
    }

    ma_engine* engine = AssetImporter::GetAudioEngine();
    if (engine == nullptr)
    {
        return false;
    }

    if (ma_sound_init_from_file(engine, resolvedPath.c_str(), 0, nullptr, nullptr, &state.sound) != MA_SUCCESS)
    {
        std::cerr << "[AudioSystem] Failed to load audio: " << resolvedPath << "\n";
        return false;
    }

    state.loadedPath = resolvedPath;
    state.initialized = true;
    return true;
}

void AudioSystem::ReleaseSound(Entity entity)
{
    if (const auto it = m_Sounds.find(entity.id); it != m_Sounds.end())
    {
        if (it->second.initialized)
        {
            ma_sound_uninit(&it->second.sound);
        }
        m_Sounds.erase(it);
    }
}
