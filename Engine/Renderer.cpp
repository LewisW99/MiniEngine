#include "pch.h"
#include "Renderer.h"
#include <vector>
#include <iostream>
#include "Math/MathConversions.h"

// Cube data
static const float cubeVerts[] = {
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f,
    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f,0.5f, 0.5f, -0.5f,0.5f, 0.5f
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
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* fs = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 color;
        void main() {
            FragColor = vec4(color, 1.0);
        }
    )";

    shader.Compile(vs, fs);
    mvpLoc = glGetUniformLocation(shader.id, "MVP");
    colorLoc = glGetUniformLocation(shader.id, "color");

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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
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


void Renderer::RenderToTexture(const EntityManager& entities,
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

    if (editorMode && snapGridVisible)
    {
        glUseProgram(0); // use fixed-function for simple grid lines
        glDisable(GL_DEPTH_TEST);
        DrawGrid(cam.position, snapStep); // grid size and step
        glEnable(GL_DEPTH_TEST);
        shader.Use();
        glBindVertexArray(vao);
    }

    //  Loop through entities and draw their transforms
    for (uint32_t id = 0; id < 10000; ++id)
    {
        Entity e{ id };
        if (!entities.IsAlive(e)) continue;

        // Only draw entities that actually have a TransformComponent
        try {
            auto& t = comps.GetComponent<TransformComponent>(e);

            glm::vec3 color = (e.id == selectedEntity.id)
                ? glm::vec3(1.0f, 0.5f, 0.0f)  // orange highlight
                : glm::vec3(0.4f, 0.8f, 0.6f);

            // Build model matrix manually using TransformComponent fields
            glm::mat4 model = glm::mat4(1.0f);

            // Apply translation
            model = glm::translate(model, glm::vec3(t.position.x, t.position.y, t.position.z));

            // Apply rotation (convert degrees to radians)
            model = glm::rotate(model, glm::radians(t.rotation.x), glm::vec3(1, 0, 0));
            model = glm::rotate(model, glm::radians(t.rotation.y), glm::vec3(0, 1, 0));
            model = glm::rotate(model, glm::radians(t.rotation.z), glm::vec3(0, 0, 1));

            // Apply scale
            model = glm::scale(model, glm::vec3(t.scale.x, t.scale.y, t.scale.z));

            // View + Projection as before
            glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.forward, cam.up);
            glm::mat4 proj = glm::perspective(glm::radians(cam.fov), cam.aspect, cam.nearPlane, cam.farPlane);

            // Combine them
            glm::mat4 mvp = proj * view * model;
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &mvp[0][0]);

           // glm::vec3 color = glm::vec3(0.4f, 0.8f, 0.6f); // simple default color
            glUniform3fv(colorLoc, 1, &color[0]);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        catch (...) {
            // Entity doesn't have this component, skip
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}