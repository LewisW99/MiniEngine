#include <iostream>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif

#include "../Engine/Core/Memory/LinearAllocator.h"
#include "../Engine/Core/Memory/StackAllocator.h"
#include "../Engine/Core/Memory/PoolAllocator.h"
#include "../Engine/ConfigReader.h"
#include "../Engine/ProfilerOverlay.h"
#include "../Engine/JobSystem.h"

#include "../Engine/ECS/EntityManager.h"
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/TransformSystem.h"

#include "../Engine/SceneCullingDemo.h"
#include "../Engine/AsyncLoader.h"
#include "../Engine/EventBus.h"
#include "../Engine/SaveGameManager.h"
#include "../Engine/SettingsManager.h"
#include "../Engine/Streaming/StreamingManager.h"

#include "../Engine/Renderer.h"
#include "../Engine/Rendering/Camera.h"
#include "Editor.h"

#include "../Engine/Scripting/LuaTest.h"

#include <GL/glew.h>
#include "imgui.h"
#include <imgui_internal.h>
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "StartupScreen.h"
#include "Editor/Managers/ProjectManager.h"
#include "Editor/Projects/RecentProjects.h"
#include "PrototypeBuilder.h"
#include "../Engine/Components/Physics/PhysicsComponent.h"
#include "../Engine/PhysicsSystem.h"
#include "../Engine/Scripting/ScriptComponent.h"
#include "../Engine/Scripting/ScriptSystem.h"
#include "../Engine/InputSystem.h"
#include "../Engine/EditorConsole.h"
#include "../Engine/Components/PlayerControllerComponent.h"
#include "../Engine/Components/AnimationComponent.h"
#include "../Engine/Components/AudioSourceComponent.h"
#include "../Engine/Components/NavAgentComponent.h"
#include "../Engine/Components/NavWaypointComponent.h"
#include "../Engine/Components/RuntimeUIComponent.h"
#include "../Engine/Systems/PlayerControllerSystem.h"
#include "../Engine/Systems/AnimationSystem.h"
#include "../Engine/Components/CameraFollowComponent.h"
#include "../Engine/Systems/CameraControllerSystem.h"
#include "../Engine/Systems/AudioSystem.h"
#include "../Engine/Systems/NavigationSystem.h"
#include "../Engine/Systems/UISystem.h"
#include "../Engine/Components/ColliderComponent.h"
#include "../Engine/Components/LightComponent.h"
#include "../Engine/Components/MaterialComponent.h"
#include "../Engine/Components/MeshComponent.h"
#include "../Engine/AssetDatabase/AssetImporter.h"
#include "../Engine/SceneSerializer.h"

//Creating memoryAllocator
Allocator* createAllocator(const std::unordered_map<std::string, std::string>& config) {
    const auto typeIt = config.find("allocator");
    const auto blockSizeIt = config.find("block_size");
    const auto numBlocksIt = config.find("num_blocks");

    const std::string type = typeIt != config.end() ? typeIt->second : "Linear";
    const size_t blockSize = blockSizeIt != config.end() ? std::stoull(blockSizeIt->second) : 1024 * 1024;
    const size_t numBlocks = numBlocksIt != config.end() ? std::stoull(numBlocksIt->second) : 256;

    if (type == "Linear")  return new LinearAllocator(blockSize);
    if (type == "Stack")   return new StackAllocator(blockSize);
    if (type == "Pool")    return new PoolAllocator(blockSize, numBlocks);
    return new LinearAllocator(blockSize);
}

namespace
{
    enum class LaunchMode
    {
        EditorShell,
        GameRuntime,
        PackagePrototype,
        CreateProject
    };

    struct RuntimeOptions
    {
        LaunchMode launchMode = LaunchMode::EditorShell;
        std::filesystem::path projectPath;
        std::filesystem::path sceneOverride;
        std::filesystem::path runtimeRoot;
        std::filesystem::path assetDir;
        std::filesystem::path configDir;
        std::string gameId;
    };

    std::filesystem::path GetExecutablePath()
    {
#ifdef _WIN32
        char pathBuffer[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, pathBuffer, MAX_PATH);
        return length > 0 ? std::filesystem::path(pathBuffer) : std::filesystem::current_path();
#else
        return std::filesystem::current_path();
#endif
    }

    bool ApplyPackagedRuntimeConfig(const std::filesystem::path& executableDir, RuntimeOptions& options)
    {
        const std::filesystem::path runtimeConfigPath = executableDir / "Config" / "runtime.cfg";
        if (!std::filesystem::exists(runtimeConfigPath))
        {
            return false;
        }

        const auto config = loadConfig(runtimeConfigPath.string());
        options.runtimeRoot = executableDir;
        options.assetDir = executableDir / "Assets";
        options.configDir = executableDir / "Config";

        if (const auto sceneIt = config.find("startup_scene"); sceneIt != config.end())
        {
            options.sceneOverride = executableDir / sceneIt->second;
        }

        if (const auto assetIt = config.find("asset_dir"); assetIt != config.end())
        {
            options.assetDir = executableDir / assetIt->second;
        }

        if (const auto configIt = config.find("config_dir"); configIt != config.end())
        {
            options.configDir = executableDir / configIt->second;
        }

        if (const auto gameIt = config.find("game_id"); gameIt != config.end())
        {
            options.gameId = gameIt->second;
        }

        options.launchMode = LaunchMode::GameRuntime;
        return !options.sceneOverride.empty();
    }

    RuntimeOptions ParseRuntimeOptions(int argc, char** argv)
    {
        RuntimeOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--game")
            {
                options.launchMode = LaunchMode::GameRuntime;
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    options.projectPath = argv[++i];
                }
            }
            else if (arg == "--project" && i + 1 < argc)
            {
                options.projectPath = argv[++i];
            }
            else if (arg == "--build-prototype" && i + 1 < argc)
            {
                options.launchMode = LaunchMode::PackagePrototype;
                options.projectPath = argv[++i];
            }
            else if (arg == "--create-project" && i + 1 < argc)
            {
                options.launchMode = LaunchMode::CreateProject;
                options.projectPath = argv[++i];
            }
            else if (arg == "--scene" && i + 1 < argc)
            {
                options.sceneOverride = argv[++i];
            }
            else if (!arg.empty() && arg[0] != '-')
            {
                const std::filesystem::path argumentPath = arg;
                if (argumentPath.extension() == ".meproj")
                {
                    options.projectPath = argumentPath;
                    options.launchMode = LaunchMode::EditorShell;
                }
            }
        }

        return options;
    }

    std::filesystem::path ResolveStartupScene(const RuntimeOptions& options)
    {
        const auto& project = ProjectManager::GetActive();

        if (!options.sceneOverride.empty())
        {
            return options.sceneOverride.is_absolute()
                ? options.sceneOverride
                : (!options.runtimeRoot.empty() ? options.runtimeRoot : project.rootPath) / options.sceneOverride;
        }

        if (!project.configPath.empty() && std::filesystem::exists(project.configPath))
        {
            const auto config = loadConfig(project.configPath.string());
            if (const auto it = config.find("startup_scene"); it != config.end())
            {
                const std::filesystem::path startup = it->second;
                return startup.is_absolute() ? startup : project.rootPath / startup;
            }
        }

        if (!project.startupScenePath.empty())
        {
            return project.startupScenePath.is_absolute()
                ? project.startupScenePath
                : project.rootPath / project.startupScenePath;
        }

        return project.scenePath;
    }

    std::optional<std::filesystem::path> FindEngineConfig(const RuntimeOptions& options)
    {
        std::vector<std::filesystem::path> candidates;

        if (!options.projectPath.empty())
        {
            const auto projectRoot = options.projectPath.has_extension()
                ? options.projectPath.parent_path()
                : options.projectPath;
            candidates.emplace_back(projectRoot / "engine.cfg");
            candidates.emplace_back(projectRoot / "Config" / "engine.cfg");
        }

        if (!options.runtimeRoot.empty())
        {
            candidates.emplace_back(options.runtimeRoot / "engine.cfg");
            if (!options.configDir.empty())
            {
                candidates.emplace_back(options.configDir / "engine.cfg");
            }
        }

        if (ProjectManager::HasActiveProject())
        {
            const auto& project = ProjectManager::GetActive();
            candidates.emplace_back(project.rootPath / "engine.cfg");
            candidates.emplace_back(project.rootPath / "Config" / "engine.cfg");
        }

        candidates.emplace_back(std::filesystem::current_path() / "engine.cfg");
        candidates.emplace_back(std::filesystem::current_path() / "Config" / "engine.cfg");
        candidates.emplace_back(std::filesystem::current_path() / "../Tests/engine.cfg");

        for (const auto& candidate : candidates)
        {
            if (!candidate.empty() && std::filesystem::exists(candidate))
            {
                return std::filesystem::weakly_canonical(candidate);
            }
        }

        return std::nullopt;
    }
}


// ------------------------------------------------------------
// Main runtime
// ------------------------------------------------------------
int main(int argc, char** argv) {
    RuntimeOptions runtimeOptions = ParseRuntimeOptions(argc, argv);
    const std::filesystem::path executablePath = GetExecutablePath();
    const std::filesystem::path executableDir = executablePath.parent_path();

    if (runtimeOptions.launchMode == LaunchMode::EditorShell &&
        executablePath.filename() == "Game.exe")
    {
        ApplyPackagedRuntimeConfig(executableDir, runtimeOptions);
    }

    const bool editorShellMode = runtimeOptions.launchMode == LaunchMode::EditorShell;
    const bool gameRuntimeMode = runtimeOptions.launchMode == LaunchMode::GameRuntime;

    if (runtimeOptions.launchMode == LaunchMode::CreateProject)
    {
        if (runtimeOptions.projectPath.empty())
        {
            std::cerr << "[Project] Missing project path for --create-project\n";
            return -1;
        }

        ProjectManager::Create(runtimeOptions.projectPath);
        std::cout << "[Project] Created project at " << runtimeOptions.projectPath << "\n";
        return 0;
    }

    if (runtimeOptions.launchMode == LaunchMode::PackagePrototype)
    {
        std::string error;
        if (!PrototypeBuilder::Build(runtimeOptions.projectPath, &error))
        {
            std::cerr << "[Packaging] " << error << "\n";
            return -1;
        }

        std::cout << "[Packaging] Prototype build created for " << runtimeOptions.projectPath << "\n";
        return 0;
    }

    // ---------------- SDL / OpenGL Init ----------------
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to init SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    std::unique_ptr<StartupScreen> startupScreen;
    std::string currentProjectPath;
    AppState appState = gameRuntimeMode ? AppState::Editor : AppState::Startup;
    if (editorShellMode)
    {
        startupScreen = std::make_unique<StartupScreen>();
        RecentProjects::Load();
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("MiniEngine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable VSync

    // ---------------- GLEW Init ----------------
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "GLEW init error: " << glewGetErrorString(glewStatus) << std::endl;
        return -1;
    }

    std::cout << "[OpenGL] Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "[GLEW] Version: " << glewGetString(GLEW_VERSION) << "\n";

    // ---------------- Renderer / Camera Init ----------------
    Renderer renderer;
    renderer.Init();

    Camera camera;
    camera.SetAspect(1280, 720);

    auto applyProjectSettings = [&](const Project& project)
        {
            EngineSettings settings;
            if (!SettingsManager::Load(project.rootPath / "Config" / "settings.json", settings))
            {
                return;
            }

            renderer.gamma = settings.runtime.gamma;
            renderer.exposure = settings.runtime.exposure;
            renderer.vignette = settings.runtime.vignette;
        };

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    if (editorShellMode)
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    bool rightMouseHeld = false;

    // ---------------- Engine Init ----------------
    const auto configPath = FindEngineConfig(runtimeOptions);
    auto config = configPath.has_value()
        ? loadConfig(configPath->string())
        : std::unordered_map<std::string, std::string>{};
    Allocator* allocator = createAllocator(config);
	std::cout << "[Main] Allocator created...\n" << "Allocator is :" << allocator;
    ProfilerOverlay profiler(allocator);
    JobSystem jobSystem(std::thread::hardware_concurrency() - 1);

    EntityManager entities(64);
    ComponentManager components;
    components.RegisterComponent<TransformComponent>("TransformComponent");
    components.RegisterComponent<PhysicsComponent>("PhysicsComponent");
    components.RegisterComponent<ScriptComponent>("ScriptComponent");
    components.RegisterComponent<PlayerControllerComponent>("PlayerControllerComponent");
    components.RegisterComponent<CameraFollowComponent>("CameraFollowComponent");
    components.RegisterComponent<ColliderComponent>("ColliderComponent");
    components.RegisterComponent<MaterialComponent>("MaterialComponent");
    components.RegisterComponent<MeshComponent>("MeshComponent");
    components.RegisterComponent<LightComponent>("LightComponent");
    components.RegisterComponent<AnimationComponent>("AnimationComponent");
    components.RegisterComponent<AudioSourceComponent>("AudioSourceComponent");
    components.RegisterComponent<NavAgentComponent>("NavAgentComponent");
    components.RegisterComponent<NavWaypointComponent>("NavWaypointComponent");
    components.RegisterComponent<RuntimeUIComponent>("RuntimeUIComponent");

	components.DumpRegisteredComponents();

    InputSystem inputSystem;
    inputSystem.Init();
   

    // TEMP bindings (Phase 1)
    inputSystem.BindAction("MoveForward", SDL_SCANCODE_W);
    inputSystem.BindAction("MoveBackward", SDL_SCANCODE_S);
    inputSystem.BindAction("MoveLeft", SDL_SCANCODE_A);
    inputSystem.BindAction("MoveRight", SDL_SCANCODE_D);
    inputSystem.BindAction("Jump", SDL_SCANCODE_SPACE);
    inputSystem.BindAction("ToggleCamera", SDL_SCANCODE_TAB);

    inputSystem.BindAxis("MoveZ", "MoveForward", "MoveBackward");
    inputSystem.BindAxis("MoveX", "MoveRight", "MoveLeft");

    ScriptSystem scriptSystem;
    scriptSystem.Init(&components, &entities);
    scriptSystem.SetInputSystem(&inputSystem);
    AudioSystem audioSystem;
    audioSystem.Init();
    scriptSystem.SetAudioSystem(&audioSystem);

	//RunLuaSmokeTest();

    AsyncLoader loader(jobSystem);
    StreamingManager streamer(jobSystem);
    int loadCounter = 0;

    bool running = true;
    SDL_Event event;
    auto last = std::chrono::high_resolution_clock::now();

    std::unique_ptr<Editor> editor;
    EntityMeta runtimeMeta;

    if (editorShellMode)
    {
        editor = std::make_unique<Editor>(&entities, &components, &renderer, &camera, &streamer, &scriptSystem, &inputSystem);

        if (!runtimeOptions.projectPath.empty() &&
            runtimeOptions.projectPath.extension() == ".meproj")
        {
            if (ProjectManager::Load(runtimeOptions.projectPath))
            {
                std::cout << "[Project] Opened project from command line: "
                    << runtimeOptions.projectPath << "\n";
                RecentProjects::Add(runtimeOptions.projectPath);
                applyProjectSettings(ProjectManager::GetActive());
                inputSystem.LoadBindings((ProjectManager::GetActive().rootPath / "Config" / "input_bindings.json").string());
                editor->LoadActiveProjectScene();
                appState = AppState::Editor;
            }
            else
            {
                std::cerr << "[Project] Failed to open project: " << runtimeOptions.projectPath << "\n";
                running = false;
            }
        }
    }
    else
    {
        if (!runtimeOptions.runtimeRoot.empty())
        {
            const std::filesystem::path configPath = runtimeOptions.configDir.empty()
                ? runtimeOptions.runtimeRoot / "Config" / "game.cfg"
                : runtimeOptions.configDir / "game.cfg";
            ProjectManager::LoadRuntimeLayout(
                runtimeOptions.gameId,
                runtimeOptions.runtimeRoot,
                runtimeOptions.assetDir.empty() ? runtimeOptions.runtimeRoot / "Assets" : runtimeOptions.assetDir,
                runtimeOptions.sceneOverride,
                configPath);
        }
        else if (runtimeOptions.projectPath.empty() || !ProjectManager::Load(runtimeOptions.projectPath))
        {
            std::cerr << "[Runtime] Failed to load project for --game mode.\n";
            running = false;
        }

        if (running)
        {
            applyProjectSettings(ProjectManager::GetActive());
            inputSystem.LoadBindings((ProjectManager::GetActive().rootPath / "Config" / "input_bindings.json").string());
            const std::filesystem::path startupScene = ResolveStartupScene(runtimeOptions);
            SceneSerializer::Load(startupScene.string(), entities, components, runtimeMeta);
            EventBus::Publish({ "SceneLoaded", 0, "Scene loaded", startupScene.string() });
            std::cout << "[Runtime] Loaded scene: " << startupScene << "\n";
        }
    }

    int windowW = 1920;
    int windowH = 1080;

    // ---------------- Main Loop ----------------
    while (running) {

        /*ImGuiIO& io = ImGui::GetIO();

        bool allowGameplayInput =
            editor.GetEngineMode() == EngineMode::Play &&
            !io.WantCaptureKeyboard &&
            !io.WantCaptureMouse;

        inputSystem.SetGameplayEnabled(allowGameplayInput);*/
        inputSystem.BeginFrame();
        const bool gameplayHotkeysActive =
            gameRuntimeMode ||
            (editor != nullptr && editor->GetEngineMode() == EngineMode::Play);
        // --- Input events ---
        float mouseDX = 0, mouseDY = 0;

      

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
            {
                //  Handle window resizing dynamically
                SDL_GetWindowSize(window, &windowW, &windowH);
                camera.SetAspect((float)windowW, (float)windowH);
            }

            if (event.type == SDL_KEYDOWN && !event.key.repeat)
            {
                inputSystem.OnKeyDown(event.key.keysym.scancode);

                if (gameplayHotkeysActive && event.key.keysym.scancode == SDL_SCANCODE_F5 && ProjectManager::HasActiveProject())
                {
                    const auto savePath = ProjectManager::GetActive().rootPath / "Saves" / "quicksave.sav";
                    std::filesystem::create_directories(savePath.parent_path());
                    SaveGameManager::Save(savePath, entities, components, runtimeMeta);
                    EditorConsole::Log("[SaveGame] Saved quicksave to " + savePath.string());
                }
                if (gameplayHotkeysActive && event.key.keysym.scancode == SDL_SCANCODE_F9 && ProjectManager::HasActiveProject())
                {
                    const auto savePath = ProjectManager::GetActive().rootPath / "Saves" / "quicksave.sav";
                    SaveGameManager::Load(savePath, entities, components, runtimeMeta);
                    EditorConsole::Log("[SaveGame] Loaded quicksave from " + savePath.string());
                }
            }
            else if (event.type == SDL_KEYUP)
            {
                inputSystem.OnKeyUp(event.key.keysym.scancode);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseHeld = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseHeld = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if (event.type == SDL_MOUSEMOTION && rightMouseHeld)
            {
                inputSystem.OnMouseMove(
                    (float)event.motion.xrel,
                    (float)event.motion.yrel
                );
            }

            if (inputSystem.Pressed("Jump"))
            {
                EditorConsole::Log("[Input] Jump pressed");
            }

            if (inputSystem.Held("MoveForward"))
            {
                EditorConsole::Log("[Input] Holding MoveForward");
            }
        }

        

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;


        const bool gameplayActive =
            gameRuntimeMode ||
            (editor != nullptr && editor->GetEngineMode() == EngineMode::Play);

        const bool editorPlayMode =
            editor != nullptr && editor->GetEngineMode() == EngineMode::Play;

        if (gameplayActive)
        {
            if (editorPlayMode)
            {
                scriptSystem.AutoReloadModifiedScripts(entities, components);
            }

            AnimationSystem::Update(
                entities,
                components,
                dt
            );

            NavigationSystem::Update(
                entities,
                components,
                dt
            );

            PlayerControllerSystem::Update(
                entities,
                components,
                inputSystem,
                camera,
                dt
            );

            PhysicsSystem::Update(
                entities,
                components,
                dt
            );

            CameraControllerSystem::Update(
                entities,
                components,
                camera,
                dt
            );

            for (EntityID id = 0; id < entities.GetMaxEntities(); ++id)
            {
                Entity e{ id }; // construct Entity properly

                if (!entities.IsAlive(e))
                    continue;

                if (!components.HasComponent<ScriptComponent>(e))
                    continue;

                auto& sc = components.GetComponent<ScriptComponent>(e);
                scriptSystem.Update(e, sc, dt);
            }

            audioSystem.Update(entities, components);
        }


        // ECS + Streaming
        
        /*if (int(SDL_GetTicks() / 500) % 2 == 0)
            loader.RequestLoad("Chunk_" + std::to_string(loadCounter++));
        loader.Update();

        streamer.SetCameraPos(camera.position.x, camera.position.z);
        streamer.Update();*/

        SDL_GetWindowSize(window, &windowW, &windowH);
        camera.SetAspect((float)windowW, (float)windowH);

        if (gameRuntimeMode)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            renderer.editorMode = false;
            renderer.snapGridVisible = false;
            renderer.RenderToScreen(entities, components, camera, windowW, windowH);
            UISystem::Draw(entities, components, windowW, windowH);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);
        }
        else
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (appState == AppState::Startup)
            {
                auto result = startupScreen->Draw();

                if (result.projectChosen && !result.projectPath.empty())
                {
                    ImGui::ClearActiveID();

                    bool projectReady = false;

                    if (result.projectPath.extension() == ".meproj")
                    {
                        projectReady = ProjectManager::Load(result.projectPath);
                    }
                    else
                    {
                        ProjectManager::Create(result.projectPath);
                        projectReady = true;
                    }

                    if (projectReady)
                    {
                        if (ProjectManager::HasActiveProject())
                        {
                            applyProjectSettings(ProjectManager::GetActive());
                            inputSystem.LoadBindings((ProjectManager::GetActive().rootPath / "Config" / "input_bindings.json").string());
                            if (editor != nullptr)
                            {
                                editor->LoadActiveProjectScene();
                            }
                        }
                        RecentProjects::Add(
                            result.projectPath.extension() == ".meproj"
                            ? result.projectPath
                            : ProjectManager::GetActive().projectFile
                        );

                        appState = AppState::Editor;
                    }
                    else
                    {
                        startupScreen->NotifyLoadError();
                    }
                }
            }
            else if (appState == AppState::Editor && editor != nullptr)
            {
                editor->Draw();
                if (editorPlayMode)
                {
                    UISystem::Draw(entities, components, windowW, windowH);
                }
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
                SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();

                SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
            }

            SDL_GL_SwapWindow(window);
        }
    }

    // ---------------- Shutdown ----------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    renderer.Shutdown();
    AssetImporter::Shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    scriptSystem.Shutdown();
    audioSystem.Shutdown();
    EventBus::Clear();
    
    delete allocator;
    return 0;
}



