#include "pch.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include "Components/Physics/PhysicsComponent.h"
#include "Rendering/ResourceManager.h"
#include "Systems/RenderSystem.h"

namespace
{
    glm::mat4 BuildDebugBoxModel(
        const glm::vec3& center,
        const glm::vec3& scale)
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, center);
        model = glm::scale(model, scale);
        return model;
    }

    glm::mat4 BuildVelocityIndicatorModel(
        const glm::vec3& origin,
        const glm::vec3& velocity)
    {
        const float speed = glm::length(velocity);
        if (speed <= 0.0001f)
        {
            return glm::mat4(1.0f);
        }

        const glm::vec3 direction = velocity / speed;
        const glm::vec3 up = std::abs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.95f
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(up, direction));
        const glm::vec3 forward = glm::normalize(glm::cross(direction, right));

        glm::mat4 rotation(1.0f);
        rotation[0] = glm::vec4(right, 0.0f);
        rotation[1] = glm::vec4(direction, 0.0f);
        rotation[2] = glm::vec4(forward, 0.0f);

        glm::mat4 model(1.0f);
        model = glm::translate(model, origin + direction * (speed * 0.5f));
        model *= rotation;
        model = glm::scale(model, glm::vec3(0.05f, speed, 0.05f));
        return model;
    }

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
        uniform int u_RenderMode;

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

            if (u_RenderMode == 1)
            {
                FragColor = vec4(baseColor, 1.0);
                return;
            }

            if (u_RenderMode == 2)
            {
                vec3 reflected = reflect(-V, N);
                FragColor = vec4(abs(reflected), 1.0);
                return;
            }

            if (u_RenderMode == 4)
            {
                FragColor = vec4(normalize(N) * 0.5 + 0.5, 1.0);
                return;
            }

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

    const char* debugVs = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        uniform mat4 u_Model;
        uniform mat4 u_ViewProj;

        void main()
        {
            gl_Position = u_ViewProj * u_Model * vec4(aPos, 1.0);
        }
    )";

    const char* debugFs = R"(
        #version 330 core

        uniform vec3 u_Color;

        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(u_Color, 1.0);
        }
    )";

    debugShader.Compile(debugVs, debugFs);

    const char* postVs = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aUV;
        out vec2 vUV;
        void main()
        {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* postFs = R"(
        #version 330 core
        in vec2 vUV;
        uniform sampler2D u_SceneTexture;
        uniform float u_Gamma;
        uniform float u_Exposure;
        uniform bool u_Vignette;
        out vec4 FragColor;

        void main()
        {
            vec3 color = texture(u_SceneTexture, vUV).rgb;
            color = vec3(1.0) - exp(-color * max(u_Exposure, 0.0001));

            if (u_Vignette)
            {
                vec2 centered = vUV * 2.0 - 1.0;
                float vignette = 1.0 - dot(centered, centered) * 0.25;
                color *= clamp(vignette, 0.35, 1.0);
            }

            color = pow(max(color, vec3(0.0)), vec3(1.0 / max(u_Gamma, 0.0001)));
            FragColor = vec4(color, 1.0);
        }
    )";

    postShader.Compile(postVs, postFs);

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
    uniforms.renderMode = glGetUniformLocation(shader.id, "u_RenderMode");
    debugUniforms.model = glGetUniformLocation(debugShader.id, "u_Model");
    debugUniforms.viewProj = glGetUniformLocation(debugShader.id, "u_ViewProj");
    debugUniforms.color = glGetUniformLocation(debugShader.id, "u_Color");
    postUniforms.sceneTexture = glGetUniformLocation(postShader.id, "u_SceneTexture");
    postUniforms.gamma = glGetUniformLocation(postShader.id, "u_Gamma");
    postUniforms.exposure = glGetUniformLocation(postShader.id, "u_Exposure");
    postUniforms.vignette = glGetUniformLocation(postShader.id, "u_Vignette");

    shader.Use();
    glUniform1i(uniforms.albedoTexture, 0);
    postShader.Use();
    glUniform1i(postUniforms.sceneTexture, 0);

    const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_PostQuadVao);
    glGenBuffers(1, &m_PostQuadVbo);
    glBindVertexArray(m_PostQuadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_PostQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

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

    if (debugShader.id != 0)
    {
        glDeleteProgram(debugShader.id);
        debugShader.id = 0;
    }

    if (postShader.id != 0)
    {
        glDeleteProgram(postShader.id);
        postShader.id = 0;
    }

    if (m_PostQuadVbo != 0)
    {
        glDeleteBuffers(1, &m_PostQuadVbo);
        m_PostQuadVbo = 0;
    }

    if (m_PostQuadVao != 0)
    {
        glDeleteVertexArrays(1, &m_PostQuadVao);
        m_PostQuadVao = 0;
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
    const Entity selectedEntity)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    EnsureFramebufferSize(width, height);
    BeginFrame(m_FBO, width, height);
    SetupCamera(cam);
    SetupLighting(entities, comps, cam);
    RenderScene(entities, comps, selectedEntity);
    EndFrame();
}

void Renderer::RenderToScreen(const EntityManager& entities,
    const ComponentManager& comps,
    const Camera& cam,
    const int width,
    const int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    EnsureFramebufferSize(width, height);
    BeginFrame(m_FBO, width, height);
    SetupCamera(cam);
    SetupLighting(entities, comps, cam);
    RenderScene(entities, comps);
    EndFrame();
    PresentToScreen(width, height);
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
    glUniform1i(uniforms.renderMode, static_cast<int>(viewMode));

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

void Renderer::BeginFrame(const GLuint framebuffer, const int width, const int height)
{
    m_ActiveFramebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
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
    m_CurrentViewProj = viewProj;

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

void Renderer::RenderScene(const EntityManager& entities, const ComponentManager& comps, const Entity selectedEntity)
{
    const bool wireframeMode = viewMode == ViewMode::Wireframe;
    if (wireframeMode)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    RenderSystem::Render(entities, comps, *this);
    DrawPhysicsDebug(entities, comps, selectedEntity, m_CurrentViewProj);
    DrawLightDebug(entities, comps, selectedEntity, m_CurrentViewProj);

    if (wireframeMode)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Renderer::DrawPhysicsDebug(const EntityManager& entities,
    const ComponentManager& comps,
    const Entity selectedEntity,
    const glm::mat4& viewProj)
{
    if (!editorMode || !collisionDebugVisible)
    {
        return;
    }

    const Mesh* debugMesh = ResourceManager::GetMesh("builtin://cube");
    if (debugMesh == nullptr || !debugMesh->IsValid())
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    debugShader.Use();
    glUniformMatrix4fv(debugUniforms.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));

    for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) ||
            !comps.HasComponent<TransformComponent>(entity) ||
            !comps.HasComponent<ColliderComponent>(entity))
        {
            continue;
        }

        const auto& transform = comps.GetComponent<TransformComponent>(entity);
        const auto& collider = comps.GetComponent<ColliderComponent>(entity);
        const glm::vec3 entityPosition(transform.position.x, transform.position.y, transform.position.z);
        const glm::vec3 colliderScale(
            transform.scale.x * collider.halfExtents.x * 2.0f,
            transform.scale.y * collider.halfExtents.y * 2.0f,
            transform.scale.z * collider.halfExtents.z * 2.0f);

        TransformComponent debugTransform = transform;
        debugTransform.scale.x *= collider.halfExtents.x * 2.0f;
        debugTransform.scale.y *= collider.halfExtents.y * 2.0f;
        debugTransform.scale.z *= collider.halfExtents.z * 2.0f;

        glm::vec3 color(0.25f, 0.9f, 1.0f);
        if (entity.id == selectedEntity.id)
        {
            color = glm::vec3(1.0f, 0.85f, 0.2f);
        }
        else if (comps.HasComponent<PhysicsComponent>(entity) &&
            comps.GetComponent<PhysicsComponent>(entity).grounded)
        {
            color = glm::vec3(0.3f, 1.0f, 0.4f);
        }

        const glm::mat4 model = BuildModelMatrix(debugTransform);
        glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(debugUniforms.color, 1, glm::value_ptr(color));
        glBindVertexArray(debugMesh->vao);
        glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);

        if (collisionGroundedVisible &&
            comps.HasComponent<PhysicsComponent>(entity) &&
            comps.GetComponent<PhysicsComponent>(entity).grounded)
        {
            const glm::vec3 groundedScale(
                colliderScale.x * 0.55f,
                std::max(colliderScale.y * 0.05f, 0.03f),
                colliderScale.z * 0.55f);
            const glm::vec3 groundedCenter(
                entityPosition.x,
                entityPosition.y - (colliderScale.y * 0.5f) - groundedScale.y,
                entityPosition.z);
            const glm::mat4 groundedModel = BuildDebugBoxModel(groundedCenter, groundedScale);
            const glm::vec3 groundedColor = entity.id == selectedEntity.id
                ? glm::vec3(1.0f, 0.95f, 0.35f)
                : glm::vec3(0.2f, 1.0f, 0.35f);

            glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(groundedModel));
            glUniform3fv(debugUniforms.color, 1, glm::value_ptr(groundedColor));
            glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);
        }

        if (collisionVelocityVisible && comps.HasComponent<PhysicsComponent>(entity))
        {
            const auto& physics = comps.GetComponent<PhysicsComponent>(entity);
            const float speed = glm::length(physics.velocity);
            if (speed > 0.01f)
            {
                const glm::mat4 velocityModel = BuildVelocityIndicatorModel(
                    entityPosition + glm::vec3(0.0f, colliderScale.y * 0.5f, 0.0f),
                    physics.velocity);
                const glm::vec3 velocityColor = entity.id == selectedEntity.id
                    ? glm::vec3(1.0f, 0.55f, 0.2f)
                    : glm::vec3(1.0f, 0.25f, 0.25f);

                glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(velocityModel));
                glUniform3fv(debugUniforms.color, 1, glm::value_ptr(velocityColor));
                glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }
    }

    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader.Use();
}

void Renderer::DrawLightDebug(const EntityManager& entities,
    const ComponentManager& comps,
    const Entity selectedEntity,
    const glm::mat4& viewProj)
{
    if (!editorMode || !lightGizmoVisible)
    {
        return;
    }

    const Mesh* debugMesh = ResourceManager::GetMesh("builtin://cube");
    if (debugMesh == nullptr || !debugMesh->IsValid())
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    debugShader.Use();
    glUniformMatrix4fv(debugUniforms.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
    glBindVertexArray(debugMesh->vao);

    for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (!entities.IsAlive(entity) ||
            !comps.HasComponent<LightComponent>(entity) ||
            !comps.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        const auto& light = comps.GetComponent<LightComponent>(entity);
        const auto& transform = comps.GetComponent<TransformComponent>(entity);
        const glm::vec3 origin = ToGlm(transform.position);
        const glm::vec3 direction = glm::length(light.direction) > 0.0001f
            ? glm::normalize(light.direction)
            : glm::vec3(0.0f, -1.0f, 0.0f);
        const bool selected = entity.id == selectedEntity.id;

        const glm::vec3 originColor = selected
            ? glm::vec3(1.0f, 0.9f, 0.3f)
            : glm::vec3(1.0f, 0.75f, 0.2f);
        const glm::vec3 beamColor = selected
            ? glm::vec3(1.0f, 0.95f, 0.5f)
            : glm::vec3(1.0f, 0.9f, 0.45f);
        const glm::vec3 targetColor = selected
            ? glm::vec3(0.95f, 0.8f, 0.2f)
            : glm::vec3(0.85f, 0.65f, 0.15f);

        const glm::mat4 originModel = BuildDebugBoxModel(origin, glm::vec3(0.25f));
        glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(originModel));
        glUniform3fv(debugUniforms.color, 1, glm::value_ptr(originColor));
        glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);

        const glm::vec3 beamVelocity = -direction * (2.5f + std::max(light.intensity, 0.25f));
        const glm::mat4 beamModel = BuildVelocityIndicatorModel(origin, beamVelocity);
        glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(beamModel));
        glUniform3fv(debugUniforms.color, 1, glm::value_ptr(beamColor));
        glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);

        const glm::vec3 targetCenter = origin - direction * 4.0f;
        const glm::mat4 targetModel = BuildDebugBoxModel(targetCenter, glm::vec3(0.7f, 0.04f, 0.7f));
        glUniformMatrix4fv(debugUniforms.model, 1, GL_FALSE, glm::value_ptr(targetModel));
        glUniform3fv(debugUniforms.color, 1, glm::value_ptr(targetColor));
        glDrawElements(GL_TRIANGLES, debugMesh->indexCount, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader.Use();
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
    m_ActiveFramebuffer = 0;
}

void Renderer::PresentToScreen(const int width, const int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!postProcessingEnabled)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    postShader.Use();
    glUniform1f(postUniforms.gamma, gamma);
    glUniform1f(postUniforms.exposure, exposure);
    glUniform1i(postUniforms.vignette, vignette ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
    glBindVertexArray(m_PostQuadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    shader.Use();
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
