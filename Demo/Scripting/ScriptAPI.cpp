#include "ScriptAPI.h"

const std::vector<ScriptApiCategory>& GetScriptAPI()
{
    static std::vector<ScriptApiCategory> api =
    {
        {
            "Transform",
            "Entity transform operations",
            {
                {
                    "Translate",
                    "Translate(x, y, z)",
                    "Moves the entity in world space by the given offset."
                },
                {
                    "SetPosition",
                    "SetPosition(x, y, z)",
                    "Sets the entity world position."
                }
            }
        },
        {
            "Debug",
            "Debug utilities",
            {
                {
                    "print",
                    "print(...)",
                    "Prints text to the editor console."
                }
            }
        }
    };

    return api;
}

