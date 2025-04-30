#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ew/shader.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/mesh.h>
#include <vector>

#include "dh/heightMap.h" // Include our heightmap class

// Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime = 0.0f;
float deltaTime = 0.0f;

const float M_PI = 3.14159265358979323846f;

// Forward declarations
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

// Camera and transforms
ew::Camera camera;
ew::CameraController cameraController;

// Heightmap settings
struct HeightmapSettings {
    glm::vec3 scale = glm::vec3(100.0f, 20.0f, 100.0f);
    GLuint texture = 0;
    bool wireframe = false;
    float ambientStrength = 0.3f;
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f));
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 0.9f);
} heightmapSettings;

// Mesh and dimensions
ew::Mesh heightmapMesh;
int heightmapWidth = 0;
int heightmapHeight = 0;

// Helper Functions
void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
    camera->position = glm::vec3(0, 50.0f, 50.0f);
    camera->target = glm::vec3(0);
    controller->yaw = -90.0f;
    controller->pitch = -45.0f;
}

void drawUI() {
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Heightmap Controls");

    if (ImGui::Button("Reset Camera")) {
        resetCamera(&camera, &cameraController);
    }

    // Scale controls
    ImGui::Text("Heightmap Scale");
    ImGui::SliderFloat("X Scale", &heightmapSettings.scale.x, 10.0f, 200.0f);
    ImGui::SliderFloat("Y Scale (Height)", &heightmapSettings.scale.y, 1.0f, 50.0f);
    ImGui::SliderFloat("Z Scale", &heightmapSettings.scale.z, 10.0f, 200.0f);

    // Lighting controls
    ImGui::Separator();
    ImGui::Text("Lighting");
    ImGui::SliderFloat("Ambient", &heightmapSettings.ambientStrength, 0.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &heightmapSettings.lightColor.x);

    float lightDirAngles[2] = {
        atan2(heightmapSettings.lightDir.x, heightmapSettings.lightDir.z) * 180.0f / M_PI,
        asin(-heightmapSettings.lightDir.y) * 180.0f / M_PI
    };

    if (ImGui::SliderFloat2("Light Direction", lightDirAngles, -180.0f, 180.0f)) {
        float horz = lightDirAngles[0] * M_PI / 180.0f;
        float vert = lightDirAngles[1] * M_PI / 180.0f;
        heightmapSettings.lightDir.x = cos(vert) * sin(horz);
        heightmapSettings.lightDir.y = -sin(vert);
        heightmapSettings.lightDir.z = cos(vert) * cos(horz);
        heightmapSettings.lightDir = glm::normalize(heightmapSettings.lightDir);
    }

    // Rendering options
    ImGui::Separator();
    ImGui::Checkbox("Wireframe", &heightmapSettings.wireframe);

    if (ImGui::Button("Regenerate Mesh")) {
        // Load the height data
        std::vector<float> heightData = dh::loadHeightmapData("assets/northamericaHeightMap.png", true);

        // Update dimensions if needed
        dh::getHeightmapDimensions("assets/northamericaHeightMap.png", heightmapWidth, heightmapHeight);

        // Create the mesh with new scale values
        heightmapMesh = dh::createHeightmapMesh(heightData, heightmapWidth, heightmapHeight, heightmapSettings.scale);
    }

    ImGui::Text("Heightmap: %dx%d", heightmapWidth, heightmapHeight);
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
    ImGui::Text("FPS: %.1f", 1.0f / deltaTime);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    screenWidth = width;
    screenHeight = height;
    camera.aspectRatio = (float)screenWidth / screenHeight;
}

GLFWwindow* initWindow(const char* title, int width, int height) {
    if (!glfwInit()) {
        printf("GLFW failed to initialize\n");
        return nullptr;
    }

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL) {
        printf("GLFW failed to create window\n");
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("GLAD failed to load OpenGL headers\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return nullptr;
    }

    // Enable depth testing and culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    return window;
}

int main() {
    GLFWwindow* window = initWindow("Heightmap Renderer", screenWidth, screenHeight);
    if (!window) return -1;

    // Shader initialization
    ew::Shader heightmapShader = ew::Shader("assets/heightmap.vert", "assets/heightmap.frag");

    // Camera setup
    camera.position = glm::vec3(0.0f, 50.0f, 100.0f);
    camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
    camera.aspectRatio = (float)screenWidth / screenHeight;
    camera.fov = 60.0f;
    resetCamera(&camera, &cameraController);

    // Create the heightmap
    std::vector<float> heightData = dh::loadHeightmapData("assets/northamericaHeightMap.png", true);

    // Get dimensions using heightmap class function
    if (!dh::getHeightmapDimensions("assets/northamericaHeightMap.png", heightmapWidth, heightmapHeight)) {
        printf("Failed to determine heightmap dimensions!\n");
        heightmapWidth = heightmapHeight = sqrt(heightData.size());
    }

    // Create the mesh
    heightmapMesh = dh::createHeightmapMesh(heightData, heightmapWidth, heightmapHeight, heightmapSettings.scale);

    // Load the texture for visualization using the existing texture loading utility
    heightmapSettings.texture = ew::loadTexture("assets/northamericaHeightMap.png");

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float time = (float)glfwGetTime();
        deltaTime = time - prevFrameTime;
        prevFrameTime = time;

        // Camera movement
        cameraController.move(window, &camera, deltaTime);

        // Clear the screen
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set wireframe mode if enabled
        if (heightmapSettings.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // Set shader uniforms
        heightmapShader.use();
        heightmapShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
        heightmapShader.setMat4("_Model", glm::mat4(1.0f)); // Identity matrix
        heightmapShader.setInt("_HeightmapTexture", 0);

        // Lighting uniforms
        heightmapShader.setFloat("_AmbientStrength", heightmapSettings.ambientStrength);
        heightmapShader.setVec3("_LightDir", heightmapSettings.lightDir);
        heightmapShader.setVec3("_LightColor", heightmapSettings.lightColor);

        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, heightmapSettings.texture);

        // Render the heightmap
        heightmapMesh.draw();

        // Draw UI
        drawUI();

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteTextures(1, &heightmapSettings.texture);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}