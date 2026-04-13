#pragma once

#include <string>
#include <vector>

struct DialogueEntry
{
    int id = 0;
    std::string text;
};

struct DialogueComponent
{
    std::vector<DialogueEntry> entries{};
    int nextEntryId = 1;

    int AllocateEntryId()
    {
        return nextEntryId++;
    }

    DialogueEntry* FindEntryById(const int id)
    {
        for (auto& entry : entries)
        {
            if (entry.id == id)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    const DialogueEntry* FindEntryById(const int id) const
    {
        for (const auto& entry : entries)
        {
            if (entry.id == id)
            {
                return &entry;
            }
        }

        return nullptr;
    }
};
