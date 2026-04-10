#pragma once

#include <string>
#include "Components/LightComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ColliderComponent.h"
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "Math/MathConversions.h"
#include "Rendering/Camera.h"
#include "Rendering/RenderTypes.h"
#include "Shader.h"
#include "TransformSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Renderer
{
public:
    enum class ViewMode
    {
        Lit = 0,
        Unlit,
        ReflectionsDebug,
        Wireframe,
        NormalsDebug
    };

    ~Renderer();

    bool snapGridVisible = false;
    bool editorMode = false;
    bool collisionDebugVisible = false;
    bool collisionGroundedVisible = true;
    bool collisionVelocityVisible = true;
    bool lightGizmoVisible = true;
    bool postProcessingEnabled = true;
    float snapStep = 1.0f;
    float gamma = 2.2f;
    float exposure = 1.0f;
    bool vignette = true;
    ViewMode viewMode = ViewMode::Lit;

    void Init();
    void Shutdown();
    void RenderToTexture(const EntityManager& entities,
        const ComponentManager& comps,
        const Camera& cam,
        int width,
        int height,
        Entity selectedEntity);
    void RenderToScreen(const EntityManager& entities,
        const ComponentManager& comps,
        const Camera& cam,
        int width,
        int height);
    void DrawRenderable(const TransformComponent& transform,
        const MeshComponent& mesh,
        const MaterialComponent* material);

    GLuint GetSceneTextureID() const { return m_SceneTexture; }

private:
    struct UniformLocations
    {
        GLint model = -1;
        GLint viewProj = -1;
        GLint albedo = -1;
        GLint specular = -1;
        GLint shininess = -1;
        GLint lightDir = -1;
        GLint lightColor = -1;
        GLint cameraPos = -1;
        GLint useTexture = -1;
        GLint albedoTexture = -1;
        GLint renderMode = -1;
    };

    struct PostUniformLocations
    {
        GLint sceneTexture = -1;
        GLint gamma = -1;
        GLint exposure = -1;
        GLint vignette = -1;
    };

    struct DebugUniformLocations
    {
        GLint model = -1;
        GLint viewProj = -1;
        GLint color = -1;
    };

    struct LightState
    {
        glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    };

    Shader shader;
    Shader debugShader;
    Shader postShader;
    UniformLocations uniforms{};
    DebugUniformLocations debugUniforms{};
    PostUniformLocations postUniforms{};
    GLuint m_FBO = 0;
    GLuint m_SceneTexture = 0;
    GLuint m_RBO = 0;
    GLuint m_CurrentTexture = 0;
    GLuint m_ActiveFramebuffer = 0;
    GLuint m_PostQuadVao = 0;
    GLuint m_PostQuadVbo = 0;
    glm::mat4 m_CurrentViewProj{ 1.0f };
    bool m_IsInitialized = false;

    void EnsureFramebufferSize(int width, int height);
    void BeginFrame(GLuint framebuffer, int width, int height);
    void SetupCamera(const Camera& cam);
    void SetupLighting(const EntityManager& entities, const ComponentManager& comps, const Camera& cam);
    void RenderScene(const EntityManager& entities, const ComponentManager& comps, Entity selectedEntity = {});
    void DrawPhysicsDebug(const EntityManager& entities, const ComponentManager& comps, Entity selectedEntity, const glm::mat4& viewProj);
    void DrawLightDebug(const EntityManager& entities, const ComponentManager& comps, Entity selectedEntity, const glm::mat4& viewProj);
    void PresentToScreen(int width, int height);
    void EndFrame();
    LightState ResolveLight(const EntityManager& entities, const ComponentManager& comps) const;
    static glm::mat4 BuildModelMatrix(const TransformComponent& transform);
    static MaterialComponent DefaultMaterial();
};
