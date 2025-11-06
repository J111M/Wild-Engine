#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Renderer/Window.hpp"

namespace Wild {
    class Camera
    {
    public:
        Camera(glm::vec3 pos = { 0, 0, -10 });
        ~Camera() {};

        void UpdateMatrix(float fov, float aspect, float nearF, float farF);
        void Input(Window& window, uint32_t width, uint32_t height, float dt);

        const glm::mat4& GetProjection() const { return m_projectionMatrix; }
        const glm::mat4& GetView() const { return m_viewMatrix; }
        const glm::mat4& GetCameraMatrix() const { return m_cameraMatrix; }
        const glm::vec3& GetPosition() const { return m_position; }

        const glm::vec2& GetNearFar() const { return m_nearFar; }
        const float GetFOV() const { return m_fov; }
        const float GetAspect() const { return m_aspect; }

        const glm::quat& GetCameraQuateurnion();
    private:
        // Prevents the camera from teleporting
        bool firstClick = true;

        // Speed and sensitivity for the camera
        float m_speed = 5.0f;
        float m_sensitivity = 80.0f;

        // Stores vectors which the camera needs to move around
        glm::vec3 m_position{};

        // The orientation of the camera
        glm::vec3 m_orientation{ 0.0f, 0.0f, -1.0f };

        glm::vec2 m_lastMousePos{ 0.0f };

        glm::vec2 m_nearFar{};
        float m_fov{};
        float m_aspect{};

        // And the up of the camera
        glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };

        glm::mat4 m_projectionMatrix{ 1.0f };
        glm::mat4 m_viewMatrix{ 1.0f };
        glm::mat4 m_cameraMatrix{ 1.0f };

        glm::quat m_quaternion;
    };
}