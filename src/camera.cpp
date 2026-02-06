#include "camera.h"
#include <iostream>

using namespace std;

Camera::Camera(int width, int height, glm::vec3 position)
{
    Camera::width = width;
    Camera::height = height;
    Position = position;
}

void Camera::ProcessInputs(GLFWwindow *window, int width, int height)
{
//=================================== HANDLE KEYBOARD INPUTS ===================================
    //Movement inputs
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        Position += speed * Orientation; // Forward
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        Position += speed * -Orientation; // Backward
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        Position += -speed * glm::cross(Orientation, glm::vec3(0.0, 1.0, 0.0)); // Left
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        Position += speed * glm::cross(Orientation, glm::vec3(0.0, 1.0, 0.0)); // Right
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        Position += speed * glm::vec3(0.0f, 1.0f, 0.0f); // Up
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        Position += speed * glm::vec3(0.0f, -1.0f, 0.0f); // Down

    // //Spawn SDF Objects
    // if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    //     cout << "SPHERE" << endl;
    // if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    //     cout << "CUBE" << endl;

    if(glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        activeBuffer = FINAL;
    if(glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        activeBuffer = NORMAL;
    if(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        activeBuffer = DISTANCE;
    if(glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        activeBuffer = ID;
    if(glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS)
        activeBuffer = STEPCOUNT;
        
//==================================== HANDLE MOUSE INPUTS ====================================
    // Handle mouse inputs
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        if (firstClick)
        {
            glfwSetCursorPos(window, (width / 2), (height / 2));
            firstClick = false;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        float xOffset = sensitivity * (float)(mouseX - (width / 2));
        float yOffset = sensitivity * (float)((height / 2) - mouseY);

        yaw += xOffset;
        pitch = glm::clamp(pitch + yOffset, -89.5f, 89.5f);

        Orientation.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        Orientation.y = sin(glm::radians(pitch));
        Orientation.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        Orientation = normalize(Orientation);

        Right = normalize(cross(Orientation, WorldUp));
        Up = cross(Right, Orientation);
        
        CameraToWorld = glm::mat3(Right, Up, Orientation);

        glfwSetCursorPos(window, (width / 2), (height / 2));
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstClick = true;
    }
}