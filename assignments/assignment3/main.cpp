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
#include <ew/mesh.h>
#include <vector>
#include <ew/procGen.h>

const int SHADOW_WIDTH = 2048;
const int SHADOW_HEIGHT = 2048;

// Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime = 0.0f;
float deltaTime = 0.0f;
ew::Mesh sphereMesh;

// Visualization mode for display (NEW)
enum class VisualizationMode {
    FINAL_RENDER,
    POSITION_BUFFER,
    NORMAL_BUFFER,
    ALBEDO_BUFFER,
    SHADOW_MAP,
    LIGHT_VISUALIZATION
};
VisualizationMode currentVisualizationMode = VisualizationMode::FINAL_RENDER;
bool showDebugUI = true;  // Toggle for debug UI

// Forward declarations
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();
void renderShadowMap(ew::Shader& depthShader, ew::Model& model, ew::Mesh& planeMesh);
void renderScene(ew::Shader& shader, ew::Model& monkeyModel, ew::Mesh& planeMesh, GLuint texture);
void drawScene(ew::Camera& camera, ew::Shader& shader, ew::Model& model, ew::Mesh& planeMesh);
void drawLights(ew::Shader& shader, ew::Mesh& sphereMesh);
void drawDebugView(ew::Shader& shader, GLuint textureID); // NEW: For visualization modes
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods); // NEW: Key handler

struct PointLight {
    glm::vec3 position;
    float radius;
    glm::vec4 color;
};
const int MAX_POINT_LIGHTS = 64;
PointLight pointLights[MAX_POINT_LIGHTS];
int currentPointLightCount = MAX_POINT_LIGHTS;

void distributePointLights() {
    // Use the same grid parameters as the monkey layout
    const float SPACING = 3.0f;
    const int GRID_SIZE = 8;
    const float START_POS = -((GRID_SIZE - 1) * SPACING) / 2.0f;

    // We'll arrange lights in an 8x8 grid just like the monkeys
    // but with small offsets so they don't overlap with monkeys
    const float OFFSET_X = SPACING * 0.5f;
    const float OFFSET_Z = SPACING * 0.5f;

    int lightIndex = 0;

    // Create a grid of lights matching the monkey grid pattern
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (lightIndex >= MAX_POINT_LIGHTS) break;

            // Position lights at grid intersections with offset
            float xPos = START_POS + x * SPACING + OFFSET_X;
            float zPos = START_POS + z * SPACING + OFFSET_Z;

            // Set light position - elevate above the monkeys
            pointLights[lightIndex].position = glm::vec3(xPos, 3.5f, zPos);

            // Assign different colors to make lights distinguishable
            switch (lightIndex % 4) {
            case 0:
                pointLights[lightIndex].color = glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
                break;
            case 1:
                pointLights[lightIndex].color = glm::vec4(0.3f, 1.0f, 0.3f, 1.0f);  // Green
                break;
            case 2:
                pointLights[lightIndex].color = glm::vec4(0.3f, 0.3f, 1.0f, 1.0f);  // Blue
                break;
            case 3:
                pointLights[lightIndex].color = glm::vec4(1.0f, 1.0f, 0.3f, 1.0f);  // Yellow
                break;
            }

            // Set a consistent light radius appropriate for the grid spacing
            pointLights[lightIndex].radius = SPACING * 0.8f;

            lightIndex++;
        }
    }

    // Ensure all remaining lights (if any) are initialized
    for (int i = lightIndex; i < MAX_POINT_LIGHTS; i++) {
        pointLights[i].position = glm::vec3(0.0f);
        pointLights[i].color = glm::vec4(1.0f);
        pointLights[i].radius = 5.0f;
    }
}struct FrameBuffer {
    GLuint fbo;
    GLuint colorBuffers[3]; // For GBuffer implementation
    GLuint color0;          // For shadow frame buffer
    GLuint color1;          // For shadow frame buffer
    GLuint depth;           // For shadow frame buffer
    unsigned int width;     // Buffer dimensions
    unsigned int height;    // Buffer dimensions
};

FrameBuffer createGBuffer(unsigned int width, unsigned int height) {
    FrameBuffer framebuffer;
    framebuffer.width = width;
    framebuffer.height = height;

    glCreateFramebuffers(1, &framebuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

    int formats[3] = {
        GL_RGB32F, //0 = World Position 
        GL_RGB16F, //1 = World Normal
        GL_RGB16F  //2 = Albedo
    };
    //Create 3 color textures
    for (size_t i = 0; i < 3; i++)
    {
        glGenTextures(1, &framebuffer.colorBuffers[i]);
        glBindTexture(GL_TEXTURE_2D, framebuffer.colorBuffers[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, formats[i], width, height);
        //Clamp to border so we don't wrap when sampling for post processing
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //Attach each texture to a different slot.
        //GL_COLOR_ATTACHMENT0 + 1 = GL_COLOR_ATTACHMENT1, etc
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, framebuffer.colorBuffers[i], 0);
    }
    //Explicitly tell OpenGL which color attachments we will draw to
    const GLenum drawBuffers[3] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
    };
    glDrawBuffers(3, drawBuffers);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return framebuffer;
}

// Camera and transforms
ew::Camera camera;
ew::CameraController cameraController;
std::vector<ew::Transform> monkeyTransforms;
ew::Transform planeTransform;
ew::Mesh plane;

// G-Buffer
FrameBuffer gBuffer;

// Shadow mapping structs and variables
struct DirectionalLight {
    glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    // Shadow mapping parameters
    float minBias = 0.001f;
    float maxBias = 0.01f;
    float softness = 0.0f;
} directionalLight;

// Frame buffer for regular rendering
FrameBuffer framebuffer;

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
    // Parameters for the orthographic projection
    float near_plane = 1.0f;
    float far_plane = 25.0f;

    glm::vec3 sceneCenter = glm::vec3(0.0f);

    glm::vec3 lightPos = -directionalLight.direction * 15.0f;

    glm::mat4 lightView = glm::lookAt(
        lightPos,
        sceneCenter,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float orthoSize = 15.0f;
    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        near_plane, far_plane
    );

    return lightProjection * lightView;
}

void renderShadowMap(ew::Shader& depthShader, ew::Model& model, ew::Mesh& planeMesh) {
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);

    glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix();
    depthShader.use();
    depthShader.setMat4("_LightSpaceMatrix", lightSpaceMatrix);

    // Render all monkeys
    for (size_t i = 0; i < monkeyTransforms.size(); i++) {
        depthShader.setMat4("_Model", monkeyTransforms[i].modelMatrix());
        model.draw();
    }

    // Render plane
    depthShader.setMat4("_Model", planeTransform.modelMatrix());
    planeMesh.draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore culling state for regular rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void setupCulling() {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void drawScene(ew::Camera& camera, ew::Shader& shader, ew::Model& model, ew::Mesh& planeMesh) {
    shader.use();

    // Camera and view
    shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
    shader.setVec3("camera_pos", camera.position);

    // Render plane
    shader.setMat4("_Model", planeTransform.modelMatrix());
    planeMesh.draw();

    glEnable(GL_DEPTH_TEST);
    // Render all monkeys
    for (size_t i = 0; i < monkeyTransforms.size(); i++) {
        shader.setMat4("_Model", monkeyTransforms[i].modelMatrix());
        model.draw();
    }
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

    // Render all 64 monkeys
    for (size_t i = 0; i < monkeyTransforms.size(); i++) {
        shader.setMat4("_Model", monkeyTransforms[i].modelMatrix());
        monkeyModel.draw();
    }

    // Render plane
    shader.setMat4("_Model", planeTransform.modelMatrix());
    planeMesh.draw();
}

// NEW: Function to draw a fullscreen quad for debug visualization
void drawDebugView(ew::Shader& shader, GLuint textureID) {
    static GLuint debugVAO = 0;

    // Create VAO for fullscreen quad if it doesn't exist
    if (debugVAO == 0) {
        glGenVertexArrays(1, &debugVAO);
    }

    shader.use();
    shader.setInt("_MainTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Draw a fullscreen triangle
    glBindVertexArray(debugVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void drawUI() {
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (showDebugUI) {
        ImGui::Begin("GBuffers"); {
            ImVec2 texSize = ImVec2(gBuffer.width / 4, gBuffer.height / 4);
            for (size_t i = 0; i < 3; i++)
            {
                ImGui::Image((ImTextureID)gBuffer.colorBuffers[i], texSize, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
        ImGui::End();

        ImGui::Begin("Shadow Mapping Settings");

        if (ImGui::Button("Reset Camera")) {
            resetCamera(&camera, &cameraController);
        }

        // NEW: Visualization mode selection
        if (ImGui::CollapsingHeader("Visualization Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::RadioButton("Final Render", currentVisualizationMode == VisualizationMode::FINAL_RENDER)) {
                currentVisualizationMode = VisualizationMode::FINAL_RENDER;
            }
            if (ImGui::RadioButton("Position Buffer", currentVisualizationMode == VisualizationMode::POSITION_BUFFER)) {
                currentVisualizationMode = VisualizationMode::POSITION_BUFFER;
            }
            if (ImGui::RadioButton("Normal Buffer", currentVisualizationMode == VisualizationMode::NORMAL_BUFFER)) {
                currentVisualizationMode = VisualizationMode::NORMAL_BUFFER;
            }
            if (ImGui::RadioButton("Albedo Buffer", currentVisualizationMode == VisualizationMode::ALBEDO_BUFFER)) {
                currentVisualizationMode = VisualizationMode::ALBEDO_BUFFER;
            }
            if (ImGui::RadioButton("Shadow Map", currentVisualizationMode == VisualizationMode::SHADOW_MAP)) {
                currentVisualizationMode = VisualizationMode::SHADOW_MAP;
            }
            if (ImGui::RadioButton("Light Visualization", currentVisualizationMode == VisualizationMode::LIGHT_VISUALIZATION)) {
                currentVisualizationMode = VisualizationMode::LIGHT_VISUALIZATION;
            }
        }

        // NEW: Control for point light count
        if (ImGui::CollapsingHeader("Point Light Settings")) {
            ImGui::SliderInt("Point Light Count", &currentPointLightCount, 0, MAX_POINT_LIGHTS);

            // NEW: Add controls for modifying point light properties
            ImGui::Text("Selected Point Light Properties");
            static int selectedLightIndex = 0;
            ImGui::SliderInt("Light Index", &selectedLightIndex, 0, currentPointLightCount - 1);

            if (selectedLightIndex >= 0 && selectedLightIndex < currentPointLightCount) {
                ImGui::SliderFloat3("Position", glm::value_ptr(pointLights[selectedLightIndex].position), -15.0f, 15.0f);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(pointLights[selectedLightIndex].color));
                ImGui::SliderFloat("Light Radius", &pointLights[selectedLightIndex].radius, 1.0f, 20.0f);
            }
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
    }

    // NEW: Add a simple status bar at the bottom of the screen
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;

        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = work_pos.x + PAD;
        window_pos.y = work_pos.y + work_size.y - PAD;
        window_pos_pivot.x = 0.0f;
        window_pos_pivot.y = 1.0f;

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (ImGui::Begin("Status Bar", nullptr, window_flags)) {
            ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
            ImGui::SameLine(100);

            const char* modeNames[] = {
                "Final Render", "Position Buffer", "Normal Buffer",
                "Albedo Buffer", "Shadow Map", "Light Visualization"
            };
            ImGui::Text("Mode: %s", modeNames[(int)currentVisualizationMode]);

            ImGui::SameLine(300);
            ImGui::Text("Press H to toggle UI");

            ImGui::SameLine(500);
            ImGui::Text("Active Lights: %d", currentPointLightCount);
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// NEW: Key callback function
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        showDebugUI = !showDebugUI;
    }

    // Number keys to switch visualization modes
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_1:
            currentVisualizationMode = VisualizationMode::FINAL_RENDER;
            break;
        case GLFW_KEY_2:
            currentVisualizationMode = VisualizationMode::POSITION_BUFFER;
            break;
        case GLFW_KEY_3:
            currentVisualizationMode = VisualizationMode::NORMAL_BUFFER;
            break;
        case GLFW_KEY_4:
            currentVisualizationMode = VisualizationMode::ALBEDO_BUFFER;
            break;
        case GLFW_KEY_5:
            currentVisualizationMode = VisualizationMode::SHADOW_MAP;
            break;
        case GLFW_KEY_6:
            currentVisualizationMode = VisualizationMode::LIGHT_VISUALIZATION;
            break;
        default:
            break;
        }
    }
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
    glfwSetKeyCallback(window, keyCallback); // NEW: Register key callback

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
    GLFWwindow* window = initWindow("Advanced Rendering", screenWidth, screenHeight);

    setupCulling();

    if (!window) return -1;

    // NEW: Shader for debug visualization
    ew::Shader debugViewShader = ew::Shader("assets/fsTriangle.vert", "assets/debugView.frag");

    // Shader initialization
    ew::Shader newShader = ew::Shader("assets/full.vert", "assets/full.frag");
    ew::Shader depthShader = ew::Shader("assets/depthmap.vert", "assets/depthmap.frag");
    ew::Shader gBufferShader = ew::Shader("assets/lit.vert", "assets/geometryPass.frag");
    ew::Shader deferredShader = ew::Shader("assets/fsTriangle.vert", "assets/deferredLit.frag");

    ew::Shader lightOrbShader = ew::Shader("assets/lightOrb.vert", "assets/lightOrb.frag");

    // Model and texture loading
    ew::Model monkeyModel("assets/suzanne.obj");
    GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");

    sphereMesh.load(ew::createSphere(0.5f, 20));
    // Camera setup
    camera.position = glm::vec3(0.0f, 10.0f, 20.0f);
    camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
    camera.aspectRatio = (float)screenWidth / screenHeight;
    camera.fov = 60.0f;

    // Create 64 monkey transforms in an 8x8 grid
    const int GRID_SIZE = 8;
    const float SPACING = 3.0f;
    const float START_POS = -((GRID_SIZE - 1) * SPACING) / 2.0f;

    for (int x = 0; x < GRID_SIZE; x++) {
        for (int z = 0; z < GRID_SIZE; z++) {
            ew::Transform transform;
            transform.position = glm::vec3(
                START_POS + x * SPACING,
                1.0f,
                START_POS + z * SPACING
            );
            transform.scale = glm::vec3(0.7f);
            monkeyTransforms.push_back(transform);
        }
    }

    // Initialize shadow map
    shadowMap.init();

    // Initialize G-Buffer
    gBuffer = createGBuffer(screenWidth, screenHeight);

    // Create a framebuffer for deferred lighting output
    framebuffer.width = screenWidth;
    framebuffer.height = screenHeight;
    glGenFramebuffers(1, &framebuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

    // Create color attachment
    glGenTextures(1, &framebuffer.color0);
    glBindTexture(GL_TEXTURE_2D, framebuffer.color0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, framebuffer.width, framebuffer.height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color0, 0);

    // Create depth attachment
    glGenRenderbuffers(1, &framebuffer.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, framebuffer.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer.width, framebuffer.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer.depth);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("ERROR: Framebuffer is not complete!\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create ground plane
    ew::Mesh plane = ew::createPlane(30, 30, 10);
    planeTransform.position = glm::vec3(0.0f, -5.0f, -5.0f);

    // Create fullscreen quad (using a single triangle that covers the screen)
    GLuint dummyVAO;
    glGenVertexArrays(1, &dummyVAO);

    // Distribute point lights
    distributePointLights();

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float time = (float)glfwGetTime();
        deltaTime = time - prevFrameTime;
        prevFrameTime = time;

        // Camera movement
        cameraController.move(window, &camera, deltaTime);

        // Update monkey rotations
        for (size_t i = 0; i < monkeyTransforms.size(); i++) {
            // Alternate rotation directions based on position in grid
            float rotationDirection = ((i % 2) == 0) ? 1.0f : -1.0f;

            monkeyTransforms[i].rotation = glm::rotate(
                monkeyTransforms[i].rotation,
                deltaTime * rotationDirection,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );
        }

        // 1. Render scene to G-Buffer
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
        glViewport(0, 0, gBuffer.width, gBuffer.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        gBufferShader.use();
        gBufferShader.setInt("_MainTexture", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, brickTexture);
        drawScene(camera, gBufferShader, monkeyModel, plane);

        // 2. Render depth map from light's perspective
        renderShadowMap(depthShader, monkeyModel, plane);

        // 3. LIGHTING PASS - Apply deferred lighting using G-Buffer data
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
        glViewport(0, 0, framebuffer.width, framebuffer.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        deferredShader.use();

        // Set lighting uniforms
        deferredShader.setVec3("_LightDirection", directionalLight.direction);
        deferredShader.setVec3("_LightColor", directionalLight.color);
        deferredShader.setFloat("_LightIntensity", directionalLight.intensity);
        deferredShader.setVec3("camera_pos", camera.position);

        // Set material properties
        deferredShader.setFloat("_Material.Ka", material.Ka);
        deferredShader.setFloat("_Material.Kd", material.Kd);
        deferredShader.setFloat("_Material.Ks", material.Ks);
        deferredShader.setFloat("_Material.Shininess", material.Shininess);

        // Set shadow mapping parameters
        deferredShader.setMat4("_LightSpaceMatrix", calculateLightSpaceMatrix());
        deferredShader.setFloat("_MinBias", directionalLight.minBias);
        deferredShader.setFloat("_MaxBias", directionalLight.maxBias);
        deferredShader.setFloat("_ShadowSoftness", directionalLight.softness);

        // Bind G-Buffer textures
        deferredShader.setInt("_gPositions", 0);
        deferredShader.setInt("_gNormals", 1);
        deferredShader.setInt("_gAlbedo", 2);
        deferredShader.setInt("_ShadowMap", 3);

        // Use only the active number of point lights (NEW)
        deferredShader.setInt("_PointLightCount", currentPointLightCount);

        // Set point light uniforms for all lights
        for (int i = 0; i < currentPointLightCount; i++) {
            std::string prefix = "_PointLights[" + std::to_string(i) + "].";
            deferredShader.setVec3(prefix + "position", pointLights[i].position);
            deferredShader.setFloat(prefix + "radius", pointLights[i].radius);
            deferredShader.setVec4(prefix + "color", pointLights[i].color);
        }

        glBindTextureUnit(0, gBuffer.colorBuffers[0]);  // Position
        glBindTextureUnit(1, gBuffer.colorBuffers[1]);  // Normal
        glBindTextureUnit(2, gBuffer.colorBuffers[2]);  // Albedo
        glBindTextureUnit(3, shadowMap.depthTexture);   // Shadow map

        // Draw fullscreen triangle
        glBindVertexArray(dummyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 4. Handle visualization modes (NEW)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Choose what to display based on current visualization mode
        switch (currentVisualizationMode) {
        case VisualizationMode::FINAL_RENDER:
            // Blit the final image from the lighting pass to the default framebuffer
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0, 0, framebuffer.width, framebuffer.height,
                0, 0, screenWidth, screenHeight,
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST
            );

            // Draw light orbs when in final render mode
            if (currentVisualizationMode == VisualizationMode::FINAL_RENDER ||
                currentVisualizationMode == VisualizationMode::LIGHT_VISUALIZATION) {
                drawLights(lightOrbShader, sphereMesh);
            }
            break;

        case VisualizationMode::POSITION_BUFFER:
            // Draw the position buffer
            drawDebugView(debugViewShader, gBuffer.colorBuffers[0]);
            break;

        case VisualizationMode::NORMAL_BUFFER:
            // Draw the normal buffer
            drawDebugView(debugViewShader, gBuffer.colorBuffers[1]);
            break;

        case VisualizationMode::ALBEDO_BUFFER:
            // Draw the albedo buffer
            drawDebugView(debugViewShader, gBuffer.colorBuffers[2]);
            break;

        case VisualizationMode::SHADOW_MAP:
            // Draw the shadow map
            drawDebugView(debugViewShader, shadowMap.depthTexture);
            break;

        case VisualizationMode::LIGHT_VISUALIZATION:
            // First, get a clean slate with a dark background
            glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use blending for additive light effects
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDisable(GL_DEPTH_TEST); // No depth testing for pure light viz

            // Draw the light orbs with more intensity
            lightOrbShader.use();
            lightOrbShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

            // Draw directional light as a big sphere in the sky
            glm::mat4 dirLightModel = glm::mat4(1.0f);
            dirLightModel = glm::translate(dirLightModel, -directionalLight.direction * 15.0f);
            dirLightModel = glm::scale(dirLightModel, glm::vec3(5.0f)); // Make it bigger

            lightOrbShader.setMat4("_Model", dirLightModel);
            lightOrbShader.setVec3("_Color", directionalLight.color * 2.0f); // Make it brighter
            sphereMesh.draw();

            // Draw all active point lights with exaggerated brightness
            for (int i = 0; i < currentPointLightCount; i++) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), pointLights[i].position);
                model = glm::scale(model, glm::vec3(pointLights[i].radius * 0.3f)); // Bigger for visualization

                lightOrbShader.setMat4("_Model", model);
                glm::vec3 lightColor = glm::vec3(pointLights[i].color) * 3.0f; // Exaggerate brightness
                lightOrbShader.setVec3("_Color", lightColor);

                sphereMesh.draw();
            }

            // Restore state
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            break;
        }

        // 5. Draw UI
        drawUI();

        // 6. Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &dummyVAO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}