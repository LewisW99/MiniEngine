#include "pch.h"
#include "Renderer.h"
#include <vector>
#include <iostream>
#include "Math/MathConversions.h"
#include "Components/MaterialComponent.h"

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

static const Vertex cubeVerts[] = {
    // +X
    { 0.5f,-0.5f,-0.5f,  1,0,0 },
    { 0.5f, 0.5f,-0.5f,  1,0,0 },
    { 0.5f, 0.5f, 0.5f,  1,0,0 },
    { 0.5f,-0.5f, 0.5f,  1,0,0 },

    // -X
    {-0.5f,-0.5f, 0.5f, -1,0,0 },
    {-0.5f, 0.5f, 0.5f, -1,0,0 },
    {-0.5f, 0.5f,-0.5f, -1,0,0 },
    {-0.5f,-0.5f,-0.5f, -1,0,0 },

    // +Y
    {-0.5f, 0.5f,-0.5f,  0,1,0 },
    {-0.5f, 0.5f, 0.5f,  0,1,0 },
    { 0.5f, 0.5f, 0.5f,  0,1,0 },
    { 0.5f, 0.5f,-0.5f,  0,1,0 },

    // -Y
    {-0.5f,-0.5f, 0.5f,  0,-1,0 },
    {-0.5f,-0.5f,-0.5f,  0,-1,0 },
    { 0.5f,-0.5f,-0.5f,  0,-1,0 },
    { 0.5f,-0.5f, 0.5f,  0,-1,0 },

    // +Z
    {-0.5f,-0.5f, 0.5f,  0,0,1 },
    { 0.5f,-0.5f, 0.5f,  0,0,1 },
    { 0.5f, 0.5f, 0.5f,  0,0,1 },
    {-0.5f, 0.5f, 0.5f,  0,0,1 },

    // -Z
    { 0.5f,-0.5f,-0.5f,  0,0,-1 },
    {-0.5f,-0.5f,-0.5f,  0,0,-1 },
    {-0.5f, 0.5f,-0.5f,  0,0,-1 },
    { 0.5f, 0.5f,-0.5f,  0,0,-1 },
};


static const unsigned int cubeIdx[] = {
    0,1,2, 2,3,0,
    4,5,6, 6,7,4,
    0,1,5, 5,4,0,
    2,3,7, 7,6,2,
    1,2,6, 6,5,1,
    3,0,4, 4,7,3
};

//Grid Rendering
static void DrawGrid(const glm::vec3& camPos, float halfSize = 20.0f, float step = 1.0f)
{
    // Snap grid origin to nearest step relative to camera
    float originX = floor(camPos.x / step) * step;
    float originZ = floor(camPos.z / step) * step;

    glBegin(GL_LINES);
    glColor3f(0.6f, 0.6f, 0.7f);

    for (float x = -halfSize; x <= halfSize; x += step)
    {
        float worldX = originX + x;
        glVertex3f(worldX, 0.01f, originZ - halfSize);
        glVertex3f(worldX, 0.01f, originZ + halfSize);
    }

    for (float z = -halfSize; z <= halfSize; z += step)
    {
        float worldZ = originZ + z;
        glVertex3f(originX - halfSize, 0.01f, worldZ);
        glVertex3f(originX + halfSize, 0.01f, worldZ);
    }

    // Highlight nearest snapped lines (like Unity axes)
    glColor3f(0.6f, 0.1f, 0.1f);
    glVertex3f(originX - halfSize, 0.0f, originZ);
    glVertex3f(originX + halfSize, 0.0f, originZ);
    glColor3f(0.1f, 0.6f, 0.1f);
    glVertex3f(originX, 0.0f, originZ - halfSize);
    glVertex3f(originX, 0.0f, originZ + halfSize);
    glLineWidth(1.5f);

    glEnd();
}


void Renderer::Init()
{
    // -----------------------------
    // Setup Shader
    // -----------------------------
    const char* vs = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;

        uniform mat4 u_Model;
        uniform mat4 u_ViewProj;

        out vec3 vNormal;
        out vec3 vWorldPos;

        void main()
        {
            mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
            vNormal = normalMatrix * aNormal;

            vec4 worldPos = u_Model * vec4(aPos, 1.0);
            vWorldPos = worldPos.xyz;

            gl_Position = u_ViewProj * worldPos;
        }
        )";

    const char* fs = R"(
        #version 330 core

        in vec3 vNormal;
        in vec3 vWorldPos;

        uniform vec3 u_Albedo;
        uniform float u_Specular;
        uniform float u_Shininess;

        uniform vec3 u_LightDir;
        uniform vec3 u_LightColor;
        uniform vec3 u_CameraPos;

        out vec4 FragColor;

        void main()
        {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(-u_LightDir);
            vec3 V = normalize(u_CameraPos - vWorldPos);
            vec3 H = normalize(L + V);

            float diffuse = max(dot(N, L), 0.0);

            float spec = pow(max(dot(N, H), 0.0), u_Shininess);
            vec3 specular = u_Specular * spec * u_LightColor;

            float ambient = 0.25;

            vec3 color =
            u_Albedo * (ambient + diffuse) +
            specular;

            // gamma correction
            color = pow(color, vec3(1.0 / 2.2));

            FragColor = vec4(color, 1.0);
        }
        )";

    shader.Compile(vs, fs);
    modelLoc = glGetUniformLocation(shader.id, "u_Model");
    viewProjLoc = glGetUniformLocation(shader.id, "u_ViewProj");

    albedoLoc = glGetUniformLocation(shader.id, "u_Albedo");
    specularLoc = glGetUniformLocation(shader.id, "u_Specular");
    shininessLoc = glGetUniformLocation(shader.id, "u_Shininess");

    lightDirLoc = glGetUniformLocation(shader.id, "u_LightDir");
    lightColorLoc = glGetUniformLocation(shader.id, "u_LightColor");
    cameraPosLoc = glGetUniformLocation(shader.id, "u_CameraPos");

    // -----------------------------
    // Buffers
    // -----------------------------
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // -----------------------------
    // Framebuffer
    // -----------------------------

    std::cout << "[Renderer] Ready for offscreen rendering.\n";
}

void Renderer::EnsureFramebufferSize(int width, int height)
{
    if (m_FBO && width == 0 && height == 0) return;

    if (m_FBO == 0) glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    if (m_SceneTexture == 0)
        glGenTextures(1, &m_SceneTexture);
    glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneTexture, 0);

    if (m_RBO == 0)
        glGenRenderbuffers(1, &m_RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[Renderer] Framebuffer incomplete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::RenderToTexture(
    const EntityManager& entities,
    const ComponentManager& comps,
    const Camera& cam,
    int width,
    int height,
    Entity selectedEntity)
{
    EnsureFramebufferSize(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.12f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.Use();
    glBindVertexArray(vao);

    // ------------------------------------------------------------
    // Global lighting uniforms (once per frame)
    // ------------------------------------------------------------
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    glm::vec3 lightColor = glm::vec3(1.0f);

    glUniform3fv(lightDirLoc, 1, &lightDir[0]);
    glUniform3fv(lightColorLoc, 1, &lightColor[0]);
    glUniform3fv(cameraPosLoc, 1, &cam.position[0]);

    // ------------------------------------------------------------
    // Optional editor grid
    // ------------------------------------------------------------
    if (editorMode && snapGridVisible)
    {
        glUseProgram(0);          // fixed-function
        glDisable(GL_DEPTH_TEST);
        DrawGrid(cam.position, snapStep);
        glEnable(GL_DEPTH_TEST);

        shader.Use();
        glBindVertexArray(vao);
    }

    // ------------------------------------------------------------
    // Camera matrices (same for all entities)
    // ------------------------------------------------------------
    glm::mat4 view = glm::lookAt(
        cam.position,
        cam.position + cam.forward,
        cam.up
    );

    glm::mat4 proj = glm::perspective(
        glm::radians(cam.fov),
        cam.aspect,
        cam.nearPlane,
        cam.farPlane
    );

    glm::mat4 viewProj = proj * view;
    glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, &viewProj[0][0]);

    // ------------------------------------------------------------
    // Draw entities
    // ------------------------------------------------------------
    for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
    {
        Entity e{ id };
        if (!entities.IsAlive(e))
            continue;

        if (!comps.HasComponent<TransformComponent>(e))
            continue;

        const auto& t = comps.GetComponent<TransformComponent>(e);

        // --------------------------------------------------------
        // Build model matrix
        // --------------------------------------------------------
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(t.position.x, t.position.y, t.position.z));
        model = glm::rotate(model, glm::radians(t.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(t.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(t.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(t.scale.x, t.scale.y, t.scale.z));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

        // --------------------------------------------------------
        // Material (per entity, with fallback)
        // --------------------------------------------------------
        glm::vec3 albedo = { 0.4f, 0.8f, 0.6f };
        float specular = 0.3f;
        float shininess = 32.0f;

        if (comps.HasComponent<MaterialComponent>(e))
        {
            const auto& mat = comps.GetComponent<MaterialComponent>(e);
            albedo = mat.albedo;
            specular = mat.specular;
            shininess = mat.shininess;
        }

        glUniform3fv(albedoLoc, 1, &albedo[0]);
        glUniform1f(specularLoc, specular);
        glUniform1f(shininessLoc, shininess);

        // --------------------------------------------------------
        // Draw cube
        // --------------------------------------------------------
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
