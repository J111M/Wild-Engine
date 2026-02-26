#include "Core/Camera.hpp"

namespace Wild
{
    Camera::Camera(glm::vec3 pos, uint32_t index)
    {
        m_position = pos;
        m_cameraIndex = index;
    }

    void Camera::UpdateMatrix(float fov, float aspect, float nearF, float farF)
    {
        if (m_movementIsActive)
        {
            m_viewMatrix = glm::lookAt(m_position, m_position + m_orientation, m_up);
            m_projectionMatrix = glm::perspective(fov, aspect, nearF, farF);

            m_cameraMatrix = m_projectionMatrix * m_viewMatrix;

            m_nearFar.x = nearF;
            m_nearFar.y = farF;
            m_fov = fov;
            m_aspect = aspect;

            m_quaternion = glm::quatLookAt(glm::normalize(m_orientation), glm::normalize(m_up));
        }
    }

    void Camera::Input(Window& window, uint32_t width, uint32_t height, float dt)
    {
        if (m_movementIsActive)
        {
            GLFWwindow* glfwWindow = window.GetWindow();

            dt = glm::clamp(dt, 0.0f, 0.0333f);

            if (glfwGetKey(glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { m_speed = 15.0f; }
            else
            {
                m_speed = 5.0f;
            }

            float velocity = m_speed * dt;

            if (glfwGetKey(glfwWindow, GLFW_KEY_S) == GLFW_PRESS) { m_position -= velocity * m_orientation; }
            if (glfwGetKey(glfwWindow, GLFW_KEY_W) == GLFW_PRESS) { m_position += velocity * m_orientation; }
            if (glfwGetKey(glfwWindow, GLFW_KEY_A) == GLFW_PRESS)
            {
                m_position -= velocity * glm::normalize(glm::cross(m_orientation, m_up));
            }
            if (glfwGetKey(glfwWindow, GLFW_KEY_D) == GLFW_PRESS)
            {
                m_position += velocity * glm::normalize(glm::cross(m_orientation, m_up));
            }

            if (glfwGetKey(glfwWindow, GLFW_KEY_E) == GLFW_PRESS) { m_position += velocity * m_up; }
            if (glfwGetKey(glfwWindow, GLFW_KEY_Q) == GLFW_PRESS) { m_position -= velocity * m_up; }

            // if (glfwGetKey(glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { speed = 0.05f; }
            // else { speed = 0.05f; }

            // Handles mouse inputs
            if (glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
            {
                // Hides the mouse cursor
                // glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

                // Get current mouse position
                double mouseX, mouseY;
                glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);

                if (firstClick)
                {
                    m_lastMousePos.x = mouseX;
                    m_lastMousePos.y = mouseY;
                    firstClick = false;
                }

                // Calculate offset (delta movement)
                float deltaX = static_cast<float>(mouseX - m_lastMousePos.x);
                float deltaY = static_cast<float>(mouseY - m_lastMousePos.y);

                // Update last mouse position
                m_lastMousePos.x = mouseX;
                m_lastMousePos.y = mouseY;

                // Rotation based on mouse movement
                float rotX = -m_sensitivity * (deltaX / static_cast<float>(width));
                float rotY = -m_sensitivity * (deltaY / static_cast<float>(height));

                // Compute new orientation
                glm::vec3 newOrientation =
                    glm::rotate(m_orientation, glm::radians(rotY), glm::normalize(glm::cross(m_orientation, m_up)));

                // Clamp y rotation
                if (!(glm::angle(newOrientation, m_up) <= glm::radians(5.0f) ||
                      glm::angle(newOrientation, -m_up) <= glm::radians(5.0f)))
                {
                    m_orientation = newOrientation;
                }

                // Apply x-axis rotation
                m_orientation = glm::rotate(m_orientation, glm::radians(rotX), m_up);

                // Keep the mouse within the window bounds
                if (mouseX <= 0 || mouseX >= width || mouseY <= 0 || mouseY >= height)
                {
                    glfwSetCursorPos(glfwWindow, width / 2.0, height / 2.0);
                    m_lastMousePos.x = width / 2.0;
                    m_lastMousePos.y = height / 2.0;
                }
            }
            else
            {
                // Make the cursor visible again
                // glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                firstClick = true;
            }
        }
    }
    const glm::quat& Camera::GetCameraQuateurnion()
    {
        ///*glm::mat3 camRot = glm::mat3(glm::normalize(glm::cross(m_orientation, m_up)), m_up, m_orientation);
        // return glm::quat_cast(camRot);*/

        return m_quaternion;
    }
} // namespace Wild
