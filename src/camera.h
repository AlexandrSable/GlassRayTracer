#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

enum BufferType
{
    FINAL, NORMAL, DISTANCE, ID, STEPCOUNT
};

class Camera
{   
    public:
        glm::vec3 Position;
        glm::vec3 Direction = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 Orientation = glm::vec3(0.0f, 0.0f, -1.0f);

        bool firstClick = true;

        int width;
        int height;

        float yaw = 0.0f; // Horizontal angle
        float pitch = 0.0f; // Vertical angle
        float speed = 1.0f;
        float sensitivity = 0.1f;

        BufferType activeBuffer = FINAL; 

        Camera(int width, int height, glm::vec3 Position);
        void UpdateMatrix(float FOV, float nearPlane, float farPlane, int uID);
        void ProcessInputs(GLFWwindow *window, int width, int height);
};

#endif