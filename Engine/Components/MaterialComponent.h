#pragma once
#include <glm/ext/vector_float3.hpp>


struct MaterialComponent
{
    glm::vec3 albedo = { 0.4f, 0.8f, 0.6f };
    float specular = 0.5f;
    float shininess = 32.0f;
};