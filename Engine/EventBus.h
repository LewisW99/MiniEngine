#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct EngineEvent
{
    std::string name;
    uint32_t entityId = 0;
    std::string message;
    std::string payload;
};

class EventBus
{
public:
    using Handler = std::function<void(const EngineEvent&)>;

    static void Subscribe(const std::string& eventName, Handler handler)
    {
        GetHandlers()[eventName].push_back(std::move(handler));
    }

    static void Publish(const EngineEvent& event)
    {
        auto& handlers = GetHandlers();
        if (const auto it = handlers.find(event.name); it != handlers.end())
        {
            for (const auto& handler : it->second)
            {
                handler(event);
            }
        }
    }

    static void Clear()
    {
        GetHandlers().clear();
    }

private:
    static std::unordered_map<std::string, std::vector<Handler>>& GetHandlers()
    {
        static std::unordered_map<std::string, std::vector<Handler>> handlers;
        return handlers;
    }
};
