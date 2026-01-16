#pragma once
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

/// ------------------------------------------------------------
/// Camera — free-look / editor camera with WASD + mouse controls
/// ------------------------------------------------------------
class Camera
{
public:
    glm::vec3 position{ 0.0f, 3.0f, 6.0f };
    glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
    glm::vec3 up{ 0.0f, 1.0f, 0.0f };

    float fov = 60.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;

    float yaw = -90.0f;   // facing -Z
    float pitch = 0.0f;

    float moveSpeed = 5.0f;     // units / sec
    float mouseSensitivity = 0.1f; // degrees / pixel

    // ------------------------------------------------------------
    // Matrix helpers
    // ------------------------------------------------------------
    glm::mat4 GetView() const { return glm::lookAt(position, position + forward, up); }
    glm::mat4 GetProjection() const { return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane); }

    // ------------------------------------------------------------
    // Update function
    // Pass in input states + deltaTime (independent of platform)
    // ------------------------------------------------------------
    void Update(bool moveForward, bool moveBackward,
        bool moveLeft, bool moveRight,
        bool moveUp, bool moveDown,
        float mouseDeltaX, float mouseDeltaY,
        float deltaTime)
    {
        // --- Mouse look ---
        yaw += mouseDeltaX * mouseSensitivity;
        pitch -= mouseDeltaY * mouseSensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        // Recalculate forward vector
        glm::vec3 dir;
        dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir.y = sin(glm::radians(pitch));
        dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward = glm::normalize(dir);

        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        glm::vec3 movement{ 0.0f };

        // --- Keyboard movement ---
        if (moveForward)  movement += forward;
        if (moveBackward) movement -= forward;
        if (moveLeft)     movement -= right;
        if (moveRight)    movement += right;
        if (moveUp)       movement += up;
        if (moveDown)     movement -= up;

        if (glm::length(movement) > 0.0f)
            position += glm::normalize(movement) * moveSpeed * deltaTime;
    }

    // ------------------------------------------------------------
    // Utility
    // ------------------------------------------------------------
    void SetAspect(float width, float height) { aspect = width / height; }

    void LookAt(const glm::vec3& target)
    {
        glm::vec3 dir = glm::normalize(target - position);
        forward = dir;

        // Derive yaw / pitch from forward
        yaw = glm::degrees(atan2(dir.z, dir.x)) - 90.0f;
        pitch = glm::degrees(asin(dir.y));
    }

    void LookAt(float x, float y, float z)
    {
        LookAt(glm::vec3(x, y, z));
    }

};
