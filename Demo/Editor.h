#pragma once
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/TransformSystem.h"
#include "../Engine/Renderer.h"
#include "../Engine/Rendering/Camera.h"
#include "../Engine/Streaming/StreamingManager.h"
#include "../Engine/ECS/EntityMeta.h"

#include "../Engine/AssetDatabase/AssetDatabase.h"
#include <imgui.h>
#include "../Engine/Scripting/ScriptSystem.h"
#include "../Engine/Scripting/ScriptTemplates.h"

enum class AppState {
    Startup,
    Editor
};

enum class EngineMode {
    Editor,
    Play
};

struct OpenScript
{
    std::string path;
    std::string contents;
    std::vector<char> buffer;
    bool dirty = false;
    bool hasError = false;
    std::string lastError;
    bool errorLogged = false;
    int highlightLine = -1;
};

struct ScriptJumpRequest
{
    bool pending = false;
    std::string scriptPath;
    int line = -1;
};

class InputSystem;

//Manages docking and ECU panels
class Editor
{
public:

    Editor(EntityManager* entities, ComponentManager* components, Renderer* renderer, Camera* camera, StreamingManager* streamer, ScriptSystem* scriptSystem, InputSystem* inputSystem);

    void Draw();

    std::string statusText;
    float statusTimer = 0.0f;

	AssetDatabase assetDB;

    const AssetInfo* selectedAsset = nullptr;

    std::filesystem::path activeScenePath;

	EngineMode GetEngineMode() const { return engineMode; }

    ScriptSystem* scriptSystem = nullptr;
    int scriptCursorPos = 0;
    ScriptJumpRequest scriptJump;

private:
    EntityManager* entityMgr = nullptr;
    ComponentManager* compMgr = nullptr;
    Renderer* renderer = nullptr;
    Camera* camera = nullptr;             
    StreamingManager* streamer = nullptr;  
    InputSystem* inputSystem = nullptr;

    Entity selectedEntity;
    EntityMeta meta;
	Entity renamingEntity;

    void DrawSceneView();
    void DrawHierarchy();
    void DrawDetails();
    void DrawAssetsPanel();
    void BeginDockSpace();
	void DrawCreateScriptPopup();
    void DrawConsoleWindow();
    void DrawScriptDocsPanel();

    void FocusScriptEditor();

    void DrawInputDebug(InputSystem& input);

    void TogglePlayMode();

    void EnterPlayMode();
	void ExitPlayMode();

    void CreateLuaScript(const std::string& name, ScriptTemplate templateType);

    GLuint LoadTextureForPreview(AssetInfo& a);

	EngineMode engineMode = EngineMode::Editor;

    bool requestCreateScriptPopup = false;
    char newScriptName[64] = "";

    std::vector<OpenScript> openScripts;
    int activeScriptIndex = -1;

    // Script editor helpers
    void DrawScriptEditor();
    void OpenScriptFile(const std::string& path);
    void SaveScript(OpenScript& script);
    ScriptTemplate selectedScriptTemplate = ScriptTemplate::Empty;

    std::string pendingScriptInsert;

    int FindOrOpenScript(const std::string& path);

    bool showInputDebug = false;

};