#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

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
#include "../Engine/Streaming/StreamingManager.h"

#include "../Engine/Renderer.h"
#include "../Engine/Rendering/Camera.h"
#include "Editor.h"

#include <GL/glew.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// ------------------------------------------------------------
// Helper
// ------------------------------------------------------------
Allocator* createAllocator(const std::unordered_map<std::string, std::string>& config) {
    std::string type = config.at("allocator");
    if (type == "Linear")  return new LinearAllocator(std::stoull(config.at("block_size")));
    if (type == "Stack")   return new StackAllocator(std::stoull(config.at("block_size")));
    if (type == "Pool")    return new PoolAllocator(std::stoull(config.at("block_size")),
        std::stoull(config.at("num_blocks")));
    return nullptr;
}

// ------------------------------------------------------------
// Main runtime
// ------------------------------------------------------------
int main() {
    // ---------------- SDL / OpenGL Init ----------------
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to init SDL: " << SDL_GetError() << std::endl;
        return -1;
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

    // ---------------- ImGui Init ----------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    bool rightMouseHeld = false;

    // ---------------- Engine Init ----------------
    auto config = loadConfig("../Tests/engine.cfg");
    Allocator* allocator = createAllocator(config);
    ProfilerOverlay profiler(allocator);
    JobSystem jobSystem(std::thread::hardware_concurrency() - 1);

    EntityManager entities(64);
    ComponentManager components;
    components.RegisterComponent<TransformComponent>();

    for (int i = 0; i < 8; ++i) {
        Entity e = entities.CreateEntity();
        TransformComponent t;
        t.position = { float(i * 2 - 7), 0, float(i * 2 - 7) };
        components.AddComponent(e, t);
    }

    AsyncLoader loader(jobSystem);
    StreamingManager streamer(jobSystem);
    int loadCounter = 0;

    bool running = true;
    SDL_Event event;
    auto last = std::chrono::high_resolution_clock::now();

    Editor editor(&entities, &components, &renderer, &camera, &streamer);

    int windowW = 1920;
    int windowH = 1080;

    // ---------------- Main Loop ----------------
    while (running) {
        // --- Input events ---
        float mouseDX = 0, mouseDY = 0;
        const Uint8* keystate = SDL_GetKeyboardState(NULL);

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

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseHeld = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseHeld = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if (event.type == SDL_MOUSEMOTION && rightMouseHeld) {
                mouseDX = (float)event.motion.xrel;
                mouseDY = (float)event.motion.yrel;
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        bool forward = keystate[SDL_SCANCODE_W];
        bool backward = keystate[SDL_SCANCODE_S];
        bool left = keystate[SDL_SCANCODE_A];
        bool right = keystate[SDL_SCANCODE_D];
        bool up = keystate[SDL_SCANCODE_E];
        bool down = keystate[SDL_SCANCODE_Q];

        camera.Update(forward, backward, left, right, up, down, mouseDX, mouseDY, dt);

        // ECS + Streaming
        TransformSystem::Update(components, jobSystem);
        /*if (int(SDL_GetTicks() / 500) % 2 == 0)
            loader.RequestLoad("Chunk_" + std::to_string(loadCounter++));
        loader.Update();

        streamer.SetCameraPos(camera.position.x, camera.position.z);
        streamer.Update();*/

        // ---------------- ImGui Frame ----------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //  Dynamically get window size each frame
        SDL_GetWindowSize(window, &windowW, &windowH);
        camera.SetAspect((float)windowW, (float)windowH);

        // Draw your editor UI (Scene view internally scales)
        editor.Draw();
        

        // ---------------- Render ImGui ----------------
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ---------------- Shutdown ----------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    delete allocator;
    return 0;
}
