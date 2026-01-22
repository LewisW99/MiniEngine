#pragma once
#include "Shader.h"
#include "Streaming/StreamingManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../Engine/Rendering/Camera.h"
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/TransformSystem.h"
#include "../Engine/Streaming/StreamingManager.h"

//main Renderer
class Renderer {
public:

    bool snapGridVisible = false;
    bool editorMode = false;
    float snapStep = 1.0f;

    void Init();
    void RenderToTexture(const EntityManager& entities,
        const ComponentManager& comps,
        const Camera& cam,
        int width,
        int height,
        Entity selectedEntity);
    GLuint GetSceneTextureID() const { return m_SceneTexture; }


private:
    Shader shader;
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLint modelLoc = -1;
    GLint viewProjLoc = -1;

    GLint albedoLoc = -1;
    GLint specularLoc = -1;
    GLint shininessLoc = -1;

    GLint lightDirLoc = -1;
    GLint lightColorLoc = -1;
    GLint cameraPosLoc = -1;

    //  Framebuffer resources
    GLuint m_FBO = 0;
    GLuint m_SceneTexture = 0;
    GLuint m_RBO = 0;

    void EnsureFramebufferSize(int width, int height);
};