#include "pch.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include "Rendering/ResourceManager.h"
#include "Systems/RenderSystem.h"

namespace
{
    void DrawGridImmediate(const glm::vec3& camPos, const float halfSize, const float step)
    {
        const float originX = std::floor(camPos.x / step) * step;
        const float originZ = std::floor(camPos.z / step) * step;

        glBegin(GL_LINES);
        glColor3f(0.6f, 0.6f, 0.7f);

        for (float x = -halfSize; x <= halfSize; x += step)
        {
            const float worldX = originX + x;
            glVertex3f(worldX, 0.01f, originZ - halfSize);
            glVertex3f(worldX, 0.01f, originZ + halfSize);
        }

        for (float z = -halfSize; z <= halfSize; z += step)
        {
            const float worldZ = originZ + z;
            glVertex3f(originX - halfSize, 0.01f, worldZ);
            glVertex3f(originX + halfSize, 0.01f, worldZ);
        }

        glColor3f(0.6f, 0.1f, 0.1f);
        glVertex3f(originX - halfSize, 0.0f, originZ);
        glVertex3f(originX + halfSize, 0.0f, originZ);
        glColor3f(0.1f, 0.6f, 0.1f);
        glVertex3f(originX, 0.0f, originZ - halfSize);
        glVertex3f(originX, 0.0f, originZ + halfSize);
        glEnd();
    }
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Init()
{
    const char* vs = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aUV;

        uniform mat4 u_Model;
        uniform mat4 u_ViewProj;

        out vec3 vNormal;
        out vec3 vWorldPos;
        out vec2 vUV;

        void main()
        {
            mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
            vNormal = normalMatrix * aNormal;

            vec4 worldPos = u_Model * vec4(aPos, 1.0);
            vWorldPos = worldPos.xyz;
            vUV = aUV;

            gl_Position = u_ViewProj * worldPos;
        }
    )";

    const char* fs = R"(
        #version 330 core

        in vec3 vNormal;
        in vec3 vWorldPos;
        in vec2 vUV;

        uniform vec3 u_Albedo;
        uniform float u_Specular;
        uniform float u_Shininess;
        uniform vec3 u_LightDir;
        uniform vec3 u_LightColor;
        uniform vec3 u_CameraPos;
        uniform bool u_UseTexture;
        uniform sampler2D u_AlbedoTex;

        out vec4 FragColor;

        void main()
        {
            vec3 baseColor = u_UseTexture
                ? texture(u_AlbedoTex, vUV).rgb
                : u_Albedo;

            vec3 N = normalize(vNormal);
            vec3 L = normalize(-u_LightDir);
            vec3 V = normalize(u_CameraPos - vWorldPos);
            vec3 H = normalize(L + V);

            float diffuse = max(dot(N, L), 0.0);
            float spec = pow(max(dot(N, H), 0.0), u_Shininess);
            vec3 specular = u_Specular * spec * u_LightColor;
            float ambient = 0.25;

            vec3 color = baseColor * (ambient + diffuse) + specular;
            color = pow(color, vec3(1.0 / 2.2));

            FragColor = vec4(color, 1.0);
        }
    )";

    shader.Compile(vs, fs);

    uniforms.model = glGetUniformLocation(shader.id, "u_Model");
    uniforms.viewProj = glGetUniformLocation(shader.id, "u_ViewProj");
    uniforms.albedo = glGetUniformLocation(shader.id, "u_Albedo");
    uniforms.specular = glGetUniformLocation(shader.id, "u_Specular");
    uniforms.shininess = glGetUniformLocation(shader.id, "u_Shininess");
    uniforms.lightDir = glGetUniformLocation(shader.id, "u_LightDir");
    uniforms.lightColor = glGetUniformLocation(shader.id, "u_LightColor");
    uniforms.cameraPos = glGetUniformLocation(shader.id, "u_CameraPos");
    uniforms.useTexture = glGetUniformLocation(shader.id, "u_UseTexture");
    uniforms.albedoTexture = glGetUniformLocation(shader.id, "u_AlbedoTex");

    shader.Use();
    glUniform1i(uniforms.albedoTexture, 0);

    m_IsInitialized = true;
    std::cout << "[Renderer] Ready for offscreen rendering.\n";
}

void Renderer::Shutdown()
{
    if (!m_IsInitialized)
    {
        return;
    }

    ResourceManager::Clear();

    if (m_RBO != 0)
    {
        glDeleteRenderbuffers(1, &m_RBO);
        m_RBO = 0;
    }

    if (m_SceneTexture != 0)
    {
        glDeleteTextures(1, &m_SceneTexture);
        m_SceneTexture = 0;
    }

    if (m_FBO != 0)
    {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }

    if (shader.id != 0)
    {
        glDeleteProgram(shader.id);
        shader.id = 0;
    }

    m_CurrentTexture = 0;
    m_IsInitialized = false;
}

void Renderer::EnsureFramebufferSize(const int width, const int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (m_FBO == 0)
    {
        glGenFramebuffers(1, &m_FBO);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    if (m_SceneTexture == 0)
    {
        glGenTextures(1, &m_SceneTexture);
    }

    glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneTexture, 0);

    if (m_RBO == 0)
    {
        glGenRenderbuffers(1, &m_RBO);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[Renderer] Framebuffer incomplete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::RenderToTexture(const EntityManager& entities,
    const ComponentManager& comps,
    const Camera& cam,
    const int width,
    const int height,
    Entity /*selectedEntity*/)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    EnsureFramebufferSize(width, height);
    BeginFrame(width, height);
    SetupCamera(cam);
    SetupLighting(entities, comps, cam);
    RenderScene(entities, comps);
    EndFrame();
}

void Renderer::DrawRenderable(const TransformComponent& transform,
    const MeshComponent& meshComponent,
    const MaterialComponent* material)
{
    const Mesh* mesh = ResourceManager::GetMesh(meshComponent.meshPath);
    if (mesh == nullptr || !mesh->IsValid())
    {
        return;
    }

    const MaterialComponent fallback = DefaultMaterial();
    const MaterialComponent& activeMaterial = material != nullptr ? *material : fallback;
    const glm::mat4 model = BuildModelMatrix(transform);

    glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(uniforms.albedo, 1, glm::value_ptr(activeMaterial.albedo));
    glUniform1f(uniforms.specular, activeMaterial.specular);
    glUniform1f(uniforms.shininess, activeMaterial.shininess);

    GLuint textureId = 0;
    const bool shouldUseTexture = activeMaterial.useTexture && !activeMaterial.albedoTexture.empty();
    if (shouldUseTexture)
    {
        textureId = ResourceManager::GetTexture(activeMaterial.albedoTexture);
    }

    const bool usingTexture = textureId != 0;
    glUniform1i(uniforms.useTexture, usingTexture ? 1 : 0);

    if (usingTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        if (m_CurrentTexture != textureId)
        {
            glBindTexture(GL_TEXTURE_2D, textureId);
            m_CurrentTexture = textureId;
        }
    }
    else if (m_CurrentTexture != 0)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        m_CurrentTexture = 0;
    }

    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
}

void Renderer::BeginFrame(const int width, const int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.12f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader.Use();
    m_CurrentTexture = 0;
}

void Renderer::SetupCamera(const Camera& cam)
{
    const glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.forward, cam.up);
    const glm::mat4 proj = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.nearPlane, cam.farPlane);
    const glm::mat4 viewProj = proj * view;

    glUniformMatrix4fv(uniforms.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(uniforms.cameraPos, 1, glm::value_ptr(cam.position));

    if (editorMode && snapGridVisible)
    {
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
        DrawGridImmediate(cam.position, 20.0f, std::max(snapStep, 0.1f));
        glEnable(GL_DEPTH_TEST);
        shader.Use();
        glUniformMatrix4fv(uniforms.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
        glUniform3fv(uniforms.cameraPos, 1, glm::value_ptr(cam.position));
    }
}

void Renderer::SetupLighting(const EntityManager& entities, const ComponentManager& comps, const Camera& cam)
{
    const LightState light = ResolveLight(entities, comps);
    glUniform3fv(uniforms.lightDir, 1, glm::value_ptr(light.direction));
    glUniform3fv(uniforms.lightColor, 1, glm::value_ptr(light.color));
    glUniform3fv(uniforms.cameraPos, 1, glm::value_ptr(cam.position));
}

void Renderer::RenderScene(const EntityManager& entities, const ComponentManager& comps)
{
    RenderSystem::Render(entities, comps, *this);
}

void Renderer::EndFrame()
{
    glBindVertexArray(0);
    if (m_CurrentTexture != 0)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        m_CurrentTexture = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Renderer::LightState Renderer::ResolveLight(const EntityManager& entities, const ComponentManager& comps) const
{
    for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) || !comps.HasComponent<LightComponent>(entity))
        {
            continue;
        }

        const auto& light = comps.GetComponent<LightComponent>(entity);
        if (!light.enabled)
        {
            continue;
        }

        const glm::vec3 direction = glm::length(light.direction) > 0.0f
            ? glm::normalize(light.direction)
            : glm::vec3(0.0f, -1.0f, 0.0f);

        return LightState{
            direction,
            light.color * light.intensity
        };
    }

    return {};
}

glm::mat4 Renderer::BuildModelMatrix(const TransformComponent& transform)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(transform.position.x, transform.position.y, transform.position.z));
    model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));
    return model;
}

MaterialComponent Renderer::DefaultMaterial()
{
    return MaterialComponent{};
}
