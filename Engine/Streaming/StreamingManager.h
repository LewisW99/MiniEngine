#pragma once
#include <unordered_map>
#include <string>
#include <mutex>
#include <iostream>
#include "../JobSystem.h"
#include "../AsyncLoader.h"

// ------------------------------------------------------------
// Simple data representing a world "chunk"
// ------------------------------------------------------------
struct WorldChunk {
    std::string name;
    bool loaded = false;
};

// ------------------------------------------------------------
// StreamingManager - manages active chunks asynchronously
// ------------------------------------------------------------
class StreamingManager {
public:
    StreamingManager(JobSystem& js) : m_Loader(js) {}

    // Simulated camera position (just X/Z)
    void SetCameraPos(float x, float z) { m_CamX = x; m_CamZ = z; }

    // Called once per frame
    void Update() {
        // Determine which chunks should be active (within 2 "units" radius)
        int cx = int(m_CamX / m_ChunkSize);
        int cz = int(m_CamZ / m_ChunkSize);

        std::lock_guard<std::mutex> lock(m_Mutex);

        for (int x = cx - 2; x <= cx + 2; ++x) {
            for (int z = cz - 2; z <= cz + 2; ++z) {
                std::string name = ChunkName(x, z);
                if (m_Chunks.find(name) == m_Chunks.end()) {
                    m_Chunks[name] = { name, false };
                    m_Loader.RequestLoad(name);
                }
            }
        }

        m_Loader.Update();

        // Fake “unloading” log every few frames
        for (auto it = m_Chunks.begin(); it != m_Chunks.end();) {
            if ((rand() % 200) == 0) {
                std::cout << "[StreamingManager] Unloaded " << it->first << "\n";
                it = m_Chunks.erase(it);
            }
            else ++it;
        }
    }

    void DebugPrintActive() const {
        std::cout << "[StreamingManager] Active chunks: " << m_Chunks.size() << "\n";
    }

    std::vector<std::string> GetActiveChunkNames() const {
        std::vector<std::string> names;
        names.reserve(m_Chunks.size());
        for (auto& [key, value] : m_Chunks)
            names.push_back(key);
        return names;
    }

private:
    std::string ChunkName(int x, int z) const { return "Chunk_" + std::to_string(x) + "_" + std::to_string(z); }

    float m_CamX = 0, m_CamZ = 0;
    const float m_ChunkSize = 50.0f;
    mutable std::mutex m_Mutex;
    std::unordered_map<std::string, WorldChunk> m_Chunks;

    AsyncLoader m_Loader;
};
