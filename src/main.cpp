#ifdef _WIN32
#include <windows.h>
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/glm.hpp>
#include <algorithm>
#include <ctime>

#include "camera.h"
#include "camera.cpp"
#include "glTFLoader.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>


const unsigned int INIT_WIDTH = 960;
const unsigned int INIT_HEIGHT = 540;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

int s_width = INIT_WIDTH;
int s_height = INIT_HEIGHT;

int MAX_TRACE_BOUNCES = 1;
int MAX_TRACE_PER_PIXEL = 1;

int   disp_fps = 0;
float disp_ms  = 0.0f;

string exeDir;

struct Sphere {
    glm::vec4  positionRadius;      // position (xyz) + radius (w)
    glm::vec4  baseColor;           // baseColor (xyz) + padding (w)
    glm::vec4  emissionColorStrength; // emissionColor (xyz) + emissionStrength (w)
};

std::vector<Sphere> spheres;

GLfloat vertices[] =
{
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f
};

GLuint indices[] =
{
    0, 2, 1,
    0, 3, 2
};



void GetExecuteDirectory(string* exeDir)
{
    char exePath[1024];

    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    string exeDirLoc = string(exePath).substr(0, string(exePath).find_last_of("/\\"));
    replace(exeDirLoc.begin(), exeDirLoc.end(), '\\', '/');

    *exeDir = exeDirLoc;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, s_width, s_height);
}

void getFrameRate(int* disp_fps, float* disp_ms)
{
    static float framesPerSecond = 0.0f;
    static int fps = 0;
    static float ms = 0.0f;
    static float lastTime = 0.0f;
    float currentTime = GetTickCount() * 0.001f;
    ++framesPerSecond;

    if (currentTime - lastTime > 1.0f)
    {
        lastTime = currentTime;
        fps = (int)framesPerSecond;
        ms = (fps > 0) ? (1000.0f / fps) : 0.0f;
        framesPerSecond = 0;
    }
    *disp_fps = fps;
    *disp_ms = ms;
}

string LoadShaderWithIncludes(const string& filename, int depth = 0) 
{
    if (depth > 8) {
        cerr << "ERROR: Shader include depth too large: " << filename << endl;
        return "";
    }
    ifstream file(filename.c_str());
    if (!file.is_open()) {
        cerr << "ERROR: Could not open shader file: " << filename << endl;
        return "";
    }
    stringstream result;
    string line;
    while (getline(file, line)) {
        // Remove leading/trailing whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first != string::npos) line = line.substr(first);
        size_t last = line.find_last_not_of(" \t");
        if (last != string::npos) line = line.substr(0, last+1);
        if (line.find("#include") == 0) {
            string includeFile = line.substr(8);
            includeFile.erase(0, includeFile.find_first_not_of(" \t\"<"));
            includeFile.erase(includeFile.find_last_not_of(" \t\">\"") + 1);
            string includePath = filename.substr(0, filename.find_last_of("/\\") + 1) + includeFile;
            result << LoadShaderWithIncludes(includePath, depth + 1);
        } else {
            result << line << "\n";
        }
    }
    return result.str();
}

GLuint compileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    // print compile log if any
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        std::string log(logLen, '\0');
        glGetShaderInfoLog(shader, logLen, NULL, &log[0]);
        std::cerr << "Shader compile log (type=" << type << "):\n" << log << std::endl;
    }
    if (status != GL_TRUE) {
        std::cerr << "Shader compile FAILED (type=" << type << ")\n";
    }

    return shader;
}

GLuint linkShaderProgram(std::initializer_list<GLuint> shaders)
{
    GLuint shaderProg = glCreateProgram();
    for (auto s: shaders) 
    {
        glAttachShader(shaderProg, s);
        glDeleteShader(s);
    }
    glLinkProgram(shaderProg);

    // print link log if any
    GLint status = GL_FALSE;
    glGetProgramiv(shaderProg, GL_LINK_STATUS, &status);
    GLint logLen = 0;
    glGetProgramiv(shaderProg, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        std::string log(logLen, '\0');
        glGetProgramInfoLog(shaderProg, logLen, NULL, &log[0]);
        std::cerr << "Program link log:\n" << log << std::endl;
    }
    if (status != GL_TRUE) {
        std::cerr << "Program link FAILED" << std::endl;
    }

    return shaderProg;
}



int main()
{
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "Refraction", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(false); // Disable/Enable V-Sync
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard nav
    io.ConfigWindowsMoveFromTitleBarOnly = true;            // Allow window moving
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 460";
    ImGui_ImplOpenGL3_Init(glsl_version);

////////////////////////////////////////// INITIALIZE OBJECTS /////////////////////////////////////////////

    Camera camera(s_width, s_height, glm::vec3(0.0f, 0.0f, -5.0f));

////////////////////////////////////// LOAD & COMPILE & LINK SHADERS //////////////////////////////////////

    GetExecuteDirectory(&exeDir);
    string vertexShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/Shaders/vertex.glsl");
    string fragmentShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/Shaders/fragment.glsl");
    string computeShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/Shaders/computeRayTracing.glsl");

    const char* vertexShaderSource = vertexShaderSourceStr.c_str();
    const char* fragmentShaderSource = fragmentShaderSourceStr.c_str();
    const char* computeShaderSource = computeShaderSourceStr.c_str();

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = linkShaderProgram({ vertexShader, fragmentShader });

    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeShaderSource);
    GLuint computeProgram = linkShaderProgram({ computeShader });
    
//////////////////////////////////// Create Texture for Compute shader ////////////////////////////////////

    GLuint screenTex;
    glCreateTextures    (GL_TEXTURE_2D, 1, &screenTex);
    glTextureParameteri (screenTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri (screenTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri (screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri (screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D  (screenTex, 1, GL_RGBA32F, s_width, s_height);
    glBindImageTexture  (0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

//////////////////////////////////// Create SSBO for Sphere Data ////////////////////////////////////

    GLuint sphereSSBO;
    glGenBuffers(1, &sphereSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSSBO);
    // Initial allocation (can hold up to 256 spheres)
    glBufferData(GL_SHADER_STORAGE_BUFFER, 256 * sizeof(Sphere), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

/////////////////////////////////////// SETUP FULLSCREEN QUAD FOR DISPLAY ///////////////////////////////////////

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    
    // Upload fullscreen quad vertices (position + UV)
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute (location 0: vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // UV attribute (location 1: vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);


    // Upload indices for fullscreen quad
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0); // Unbind for now

///////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Add a default test sphere for debugging
    spheres.push_back({
        glm::vec4(0.0f, 0.0f, 5.0f, 1.0f),           // positionRadius
        glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),           // baseColor
        glm::vec4(1.0f, 1.0f, 1.0f, 2.0f)            // emissionColorStrength
    });

    while(!glfwWindowShouldClose(window))
    {
        glfwGetFramebufferSize(window, &s_width, &s_height);
        glfwGetWindowSize(window, &s_width, &s_height);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Process camera inputs (WASD for movement, Right mouse for look)
        camera.ProcessInputs(window, s_width, s_height);

        // Run compute shader
        glUseProgram(computeProgram);
        
        // Update sphere SSBO
        if (!spheres.empty()) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSSBO);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, spheres.size() * sizeof(Sphere), spheres.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
        
        // Set all uniforms BEFORE dispatch
        glUniform2f(glGetUniformLocation(computeProgram, "resolution"), (float)s_width, (float)s_height);
        glUniform3f(glGetUniformLocation(computeProgram, "cameraPosition"), camera.Position.x, camera.Position.y, camera.Position.z);
        glUniformMatrix3fv(glGetUniformLocation(computeProgram, "cameraRotation"), 1, GL_FALSE, glm::value_ptr(camera.CameraToWorld));
        glUniform1f(glGetUniformLocation(computeProgram, "fov"), 90.0f);
        glUniform1i(glGetUniformLocation(computeProgram, "numSpheres"), (int)spheres.size());
        glUniform1i(glGetUniformLocation(computeProgram, "MAX_TRACE_BOUNCES"), MAX_TRACE_BOUNCES);
        glUniform1i(glGetUniformLocation(computeProgram, "MAX_TRACE_PER_PIXEL"), MAX_TRACE_PER_PIXEL);
        
        // Now dispatch the compute shader
        glDispatchCompute((GLuint)(s_width + 15) / 16, (GLuint)(s_height + 15) / 16, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Render fullscreen quad with the result texture
        glUseProgram(shaderProgram);
        glBindTextureUnit(0, screenTex);
        glUniform1i(glGetUniformLocation(shaderProgram, "screenTexture"), 0);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render debug UI
        ImGui::Begin("Debug Info");
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera.Position.x, camera.Position.y, camera.Position.z);
        ImGui::Text("Camera Yaw: %.2f", camera.yaw);
        ImGui::Text("Camera Pitch: %.2f", camera.pitch);
        ImGui::Text("FPS: %d", disp_fps);

        ImGui::DragFloat("Camera Speed", &camera.speed, 0.01f, 0.01f, 1.0f);
        ImGui::DragInt("Max Trace Bounces", &MAX_TRACE_BOUNCES, 1, 1, 200);
        ImGui::DragInt("Max Traces Per Pixel", &MAX_TRACE_PER_PIXEL, 1, 1, 200);
        ImGui::End();

        ImGui::Begin("Spheres");
        ImGui::Text("Total Spheres: %zu", spheres.size());
        
        if (ImGui::Button("Add Sphere")) {
            spheres.push_back({
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),   // positionRadius
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),   // baseColor
                glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)    // emissionColorStrength
            });
        }
        ImGui::Separator();

        for (int i = 0; i < (int)spheres.size(); ++i) {
            std::string sphereLabel = "Sphere " + std::to_string(i);
            if (ImGui::TreeNode(sphereLabel.c_str())) {
                ImGui::DragFloat3   (("Position##" + std::to_string(i)).c_str(), glm::value_ptr(spheres[i].positionRadius), 0.01f);
                ImGui::DragFloat    (("Radius##"   + std::to_string(i)).c_str(), &spheres[i].positionRadius.w, 0.01f, 0.1f, 10.0f);
                ImGui::ColorEdit3   (("Color##"    + std::to_string(i)).c_str(), glm::value_ptr(spheres[i].baseColor));
                ImGui::ColorEdit3   (("Emission##" + std::to_string(i)).c_str(), glm::value_ptr(spheres[i].emissionColorStrength));
                ImGui::DragFloat    (("Strength##" + std::to_string(i)).c_str(), &spheres[i].emissionColorStrength.w, 0.01f, 0.0f, 10.0f);

                if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
                    spheres.erase(spheres.begin() + i);
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        getFrameRate(&disp_fps, &disp_ms);
        glfwSetWindowTitle(window, ("Project: Refraction - fps: " + to_string(disp_fps) + " | ms: "+ to_string(disp_ms)).c_str());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
