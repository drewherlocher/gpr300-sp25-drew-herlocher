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
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/procGen.h>

const int SHADOW_WIDTH = 2048;
const int SHADOW_HEIGHT = 2048;

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

// Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime = 0.0f;
float deltaTime = 0.0f;

ew::Camera camera;
ew::CameraController cameraController;

ew::Transform monkeyTransform;
ew::Transform planeTransform;
ew::Mesh plane;

// Shadow mapping structs and variables
struct DirectionalLight {
    glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    // Shadow mapping parameters
    float minBias = 0.0005f;
    float maxBias = 0.005f;
    float softness = 1.0f;
} directionalLight;

struct ShadowMap {
    GLuint fbo;
    GLuint depthTexture;

    void init() {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &depthTexture);

        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("ERROR: Framebuffer is not complete!\n");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
} shadowMap;

// Material for rendering
struct Material {
    float Ka = 1.0f;
    float Kd = 0.5f;
    float Ks = 0.5f;
    float Shininess = 128.0f;
} material;

// Helper Functions
void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
    camera->position = glm::vec3(0, 3.0f, 5.0f);
    camera->target = glm::vec3(0);
    controller->yaw = controller->pitch = 0;
}

glm::mat4 calculateLightSpaceMatrix() {
    float near = 1.0f;
    float far = 7.5f;
    float size = 5.0f;

    glm::mat4 lightProjection = glm::ortho(-size, size, -size, size, near, far);

    glm::mat4 lightView = glm::lookAt(
        -directionalLight.direction * 5.0f,
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    return lightProjection * lightView;
}

void renderShadowMap(ew::Shader& depthShader, ew::Model& monkeyModel, ew::Mesh& planeMesh) {
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix();

    depthShader.use();
    depthShader.setMat4("_LightSpaceMatrix", lightSpaceMatrix);

    // Render monkey
    depthShader.setMat4("_Model", monkeyTransform.modelMatrix());
    monkeyModel.draw();

    // Render plane
    glm::mat4 planeModel = glm::mat4(1.0f);
    depthShader.setMat4("_Model", planeModel);
    planeMesh.draw();
	glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderScene(ew::Shader& shader, ew::Model& monkeyModel, ew::Mesh& planeMesh, GLuint texture) {
    shader.use();

    // Texture
    shader.setInt("_MainTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Shadow map
    shader.setInt("_ShadowMap", 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap.depthTexture);

    // Light space matrix
    glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix();
    shader.setMat4("_LightSpaceMatrix", lightSpaceMatrix);

    // Light direction and shadow parameters
    shader.setVec3("_LightDirection", directionalLight.direction);
    shader.setFloat("_MinBias", directionalLight.minBias);
    shader.setFloat("_MaxBias", directionalLight.maxBias);
    shader.setFloat("_ShadowSoftness", directionalLight.softness);

    // Camera and view
    shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
    shader.setVec3("camera_pos", camera.position);

    // Material properties
    shader.setFloat("_Material.Ka", material.Ka);   
    shader.setFloat("_Material.Kd", material.Kd);
    shader.setFloat("_Material.Ks", material.Ks);
    shader.setFloat("_Material.Shininess", material.Shininess);

    // Render monkey
    shader.setMat4("_Model", monkeyTransform.modelMatrix());
    monkeyModel.draw();

    // Render plane
    glm::mat4 planeModel = glm::mat4(1.0f);
    shader.setMat4("_Model", planeTransform.modelMatrix());
    planeMesh.draw();
}

void drawUI() {
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Shadow Mapping Settings");

    if (ImGui::Button("Reset Camera")) {
        resetCamera(&camera, &cameraController);
    }

    if (ImGui::CollapsingHeader("Light Direction")) {
        ImGui::SliderFloat3("Direction", glm::value_ptr(directionalLight.direction), -1.0f, 1.0f);
        directionalLight.direction = glm::normalize(directionalLight.direction);
    }

    if (ImGui::CollapsingHeader("Shadow Settings")) {
        ImGui::SliderFloat("Min Bias", &directionalLight.minBias, 0.0001f, 0.01f);
        ImGui::SliderFloat("Max Bias", &directionalLight.maxBias, 0.0001f, 0.1f);
        ImGui::SliderFloat("Shadow Softness", &directionalLight.softness, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Material Properties")) {
        ImGui::SliderFloat("Ambient", &material.Ka, 0.0f, 1.0f);
        ImGui::SliderFloat("Diffuse", &material.Kd, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &material.Ks, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 256.0f);
    }

    // Shadow map debug view
    ImGui::Text("Shadow Map Debug View");
    ImGui::Image((ImTextureID)(intptr_t)shadowMap.depthTexture, ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));

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
    GLFWwindow* window = initWindow("Shadow Mapping", screenWidth, screenHeight);
    if (!window) return -1;

    // Shader initialization
    ew::Shader newShader = ew::Shader("assets/full.vert", "assets/full.frag");
    ew::Shader depthShader = ew::Shader("assets/depthmap.vert", "assets/depthmap.frag");

    // Model and texture loading
    ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
    GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");

    // Camera setup
    camera.position = glm::vec3(0.0f, 3.0f, 5.0f);
    camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
    camera.aspectRatio = (float)screenWidth / screenHeight;
    camera.fov = 60.0f;

    // Initialize shadow map
    shadowMap.init();

    // Create ground plane
    ew::Mesh plane = ew::createPlane(20, 20, 10);
    planeTransform.position = glm::vec3(0.0f, -5.0f, 0.0f);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float time = (float)glfwGetTime();
        deltaTime = time - prevFrameTime;
        prevFrameTime = time;

        // Camera movement
        cameraController.move(window, &camera, deltaTime);

        // Update monkey rotation
        monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));

        // 1. Render depth map from light's perspective
        renderShadowMap(depthShader, monkeyModel, plane);

        // 2. Render scene with shadow mapping
        glViewport(0, 0, screenWidth, screenHeight);
        glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderScene(newShader, monkeyModel, plane, brickTexture);

        // Draw UI
        drawUI();

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}