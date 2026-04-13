#include "ScriptAPI.h"

const std::vector<ScriptApiCategory>& GetScriptAPI()
{
    static const std::vector<ScriptApiCategory> api =
    {
        {
            "Transform",
            "Entity transform operations",
            {
                { "GetPosition", "Transform.GetPosition(self) -> x, y, z", "Returns the entity world position.", "local x, y, z = Transform.GetPosition(self)" },
                { "SetPosition", "Transform.SetPosition(self, x, y, z)", "Sets the entity world position.", "Transform.SetPosition(self, 0, 1, 0)" },
                { "GetRotation", "Transform.GetRotation(self) -> x, y, z", "Returns the entity Euler rotation in degrees.", "local rx, ry, rz = Transform.GetRotation(self)" },
                { "SetRotation", "Transform.SetRotation(self, x, y, z)", "Sets the entity Euler rotation in degrees.", "Transform.SetRotation(self, 0, 90, 0)" },
                { "Translate", "Transform.Translate(self, x, y, z)", "Moves the entity in world space by the given offset.", "Transform.Translate(self, 0, 0, 2 * dt)" },
                { "Rotate", "Transform.Rotate(self, x, y, z)", "Adds Euler rotation in degrees.", "Transform.Rotate(self, 0, 45 * dt, 0)" }
            }
        },
        {
            "Entity",
            "Entity lifetime, queries, and spawning",
            {
                { "Spawn", "Entity.Spawn() -> entity", "Creates a new entity with a TransformComponent and returns it.", "local spawned = Entity.Spawn()" },
                { "Destroy", "Entity.Destroy(entity)", "Destroys an entity safely.", "Entity.Destroy(self)" },
                { "HasComponent", "Entity.HasComponent(entity, componentName) -> bool", "Checks for common components like Transform, Mesh, Material, Light, Physics, Collider, Tag, Script, and Audio.", "if Entity.HasComponent(self, \"Physics\") then print(\"physics\") end" },
                { "GetTag", "Entity.GetTag(entity) -> tag", "Returns the runtime tag for the entity if present.", "local tag = Entity.GetTag(self)" },
                { "SetTag", "Entity.SetTag(entity, tag)", "Creates or updates the runtime tag component.", "Entity.SetTag(self, \"Pickup\")" }
            }
        },
        {
            "Physics",
            "Physics state and motion",
            {
                { "SetVelocity", "Physics.SetVelocity(entity, x, y, z)", "Sets the current velocity.", "Physics.SetVelocity(self, 0, 6, 0)" },
                { "AddImpulse", "Physics.AddImpulse(entity, x, y, z)", "Adds to the current velocity.", "Physics.AddImpulse(self, 0, 3, 0)" },
                { "IsGrounded", "Physics.IsGrounded(entity) -> bool", "Returns whether the entity is grounded.", "if Physics.IsGrounded(self) then print(\"grounded\") end" },
                { "SetEnabled", "Physics.SetEnabled(entity, enabled)", "Enables or disables the PhysicsComponent if present.", "Physics.SetEnabled(self, false)" },
                { "IsEnabled", "Physics.IsEnabled(entity) -> bool", "Returns whether physics is enabled.", "local active = Physics.IsEnabled(self)" },
                { "IsTouchingTag", "Physics.IsTouchingTag(entity, tag) -> bool", "Returns true when the entity is currently overlapping another collider with the given tag.", "if Physics.IsTouchingTag(self, \"Door\") then print(\"door\") end" }
            }
        },
        {
            "Material",
            "Material values for renderable entities",
            {
                { "GetAlbedo", "Material.GetAlbedo(entity) -> r, g, b", "Returns the current material color.", "local r, g, b = Material.GetAlbedo(self)" },
                { "SetAlbedo", "Material.SetAlbedo(entity, r, g, b)", "Sets the material color.", "Material.SetAlbedo(self, 1.0, 0.5, 0.2)" },
                { "GetSpecular", "Material.GetSpecular(entity) -> value", "Returns the current specular value.", "local spec = Material.GetSpecular(self)" },
                { "SetSpecular", "Material.SetSpecular(entity, value)", "Sets the specular value.", "Material.SetSpecular(self, 0.8)" },
                { "GetShininess", "Material.GetShininess(entity) -> value", "Returns the current shininess.", "local shine = Material.GetShininess(self)" },
                { "SetShininess", "Material.SetShininess(entity, value)", "Sets the material shininess.", "Material.SetShininess(self, 64.0)" },
                { "GetTexture", "Material.GetTexture(entity) -> path", "Returns the current albedo texture path.", "local path = Material.GetTexture(self)" },
                { "SetTexture", "Material.SetTexture(entity, path)", "Assigns an albedo texture path and enables texture sampling.", "Material.SetTexture(self, \"Assets/Textures/crate.png\")" },
                { "UseTexture", "Material.UseTexture(entity) -> bool", "Returns whether texture sampling is enabled.", "local enabled = Material.UseTexture(self)" },
                { "SetUseTexture", "Material.SetUseTexture(entity, enabled)", "Enables or disables texture sampling.", "Material.SetUseTexture(self, true)" }
            }
        },
        {
            "Light",
            "Directional light controls",
            {
                { "GetDirection", "Light.GetDirection(entity) -> x, y, z", "Returns the light direction.", "local x, y, z = Light.GetDirection(self)" },
                { "SetDirection", "Light.SetDirection(entity, x, y, z)", "Sets the light direction.", "Light.SetDirection(self, -0.4, -1.0, -0.2)" },
                { "GetColor", "Light.GetColor(entity) -> r, g, b", "Returns the light color.", "local r, g, b = Light.GetColor(self)" },
                { "SetColor", "Light.SetColor(entity, r, g, b)", "Sets the light color.", "Light.SetColor(self, 1.0, 0.8, 0.6)" },
                { "GetIntensity", "Light.GetIntensity(entity) -> value", "Returns light intensity.", "local intensity = Light.GetIntensity(self)" },
                { "SetIntensity", "Light.SetIntensity(entity, value)", "Sets light intensity.", "Light.SetIntensity(self, 2.0)" }
            }
        },
        {
            "Audio",
            "Audio playback helpers",
            {
                { "Play", "Audio.Play(entity)", "Starts the entity's AudioSourceComponent.", "Audio.Play(self)" },
                { "Stop", "Audio.Stop(entity)", "Stops the entity's AudioSourceComponent.", "Audio.Stop(self)" },
                { "PlayOneShot", "Audio.PlayOneShot(path)", "Plays a one-shot audio file relative to the project.", "Audio.PlayOneShot(\"Assets/Audio/pickup.wav\")" }
            }
        },
        {
            "Scene",
            "Runtime scene loading helpers for menu and flow control",
            {
                { "LoadByBuildIndex", "Scene.LoadByBuildIndex(index)", "Queues a runtime scene change using the included build-scene index.", "function OnPlayButtonClicked(self, buildIndex)\n    Scene.LoadByBuildIndex(buildIndex)\nend" },
                { "LoadByName", "Scene.LoadByName(name)", "Queues a runtime scene change using the scene asset name.", "function OnPlayButtonClicked(self, sceneName)\n    Scene.LoadByName(sceneName)\nend" }
            }
        },
        {
            "Dialogue",
            "Dialogue entry lookup for authored conversations",
            {
                { "GetEntryCount", "Dialogue.GetEntryCount(entity) -> count", "Returns the number of authored dialogue entries on the entity.", "local count = Dialogue.GetEntryCount(self)" },
                { "GetEntryByIndex", "Dialogue.GetEntryByIndex(entity, index) -> id, text", "Returns the stable entry id and text at the ordered index.", "local id, text = Dialogue.GetEntryByIndex(self, 0)" },
                { "GetEntryText", "Dialogue.GetEntryText(entity, entryId) -> text", "Looks up a dialogue entry by its stable id.", "local text = Dialogue.GetEntryText(self, 3)" }
            }
        },
        {
            "Input",
            "Input polling helpers exposed to Lua",
            {
                { "Pressed", "Input.Pressed(action) -> bool", "Returns true on the frame an action was pressed.", "if Input.Pressed(\"Jump\") then print(\"jump\") end" },
                { "Held", "Input.Held(action) -> bool", "Returns true while an action is held.", "if Input.Held(\"MoveForward\") then print(\"move\") end" },
                { "Released", "Input.Released(action) -> bool", "Returns true on the frame an action was released.", "if Input.Released(\"Jump\") then print(\"released\") end" },
                { "MouseDX", "Input.MouseDX() -> value", "Returns relative mouse delta X.", "local dx = Input.MouseDX()" },
                { "MouseDY", "Input.MouseDY() -> value", "Returns relative mouse delta Y.", "local dy = Input.MouseDY()" },
                { "MoveForward", "Input.MoveForward() -> value", "Returns the mapped forward/back axis.", "local moveZ = Input.MoveForward()" },
                { "MoveRight", "Input.MoveRight() -> value", "Returns the mapped right/left axis.", "local moveX = Input.MoveRight()" },
                { "JumpPressed", "Input.JumpPressed() -> bool", "Convenience helper for the Jump action.", "if Input.JumpPressed() then print(\"jump\") end" },
                { "ToggleCameraPressed", "Input.ToggleCameraPressed() -> bool", "Convenience helper for the ToggleCamera action.", "if Input.ToggleCameraPressed() then print(\"camera\") end" }
            }
        },
        {
            "Debug",
            "Debug utilities",
            {
                { "print", "print(...)", "Prints text to the editor console.", "print(\"Hello from Lua\")" }
            }
        }
    };

    return api;
}
