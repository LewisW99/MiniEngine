#pragma once

#include <cstdint>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
    glm::vec2 uv{ 0.0f, 0.0f };
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    bool IsValid() const
    {
        return !vertices.empty() && !indices.empty();
    }
};

struct Mesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    bool IsValid() const
    {
        return vao != 0 && indexCount > 0;
    }
};
