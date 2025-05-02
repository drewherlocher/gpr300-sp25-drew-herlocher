#include <stdio.h>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <ew/external/glad.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ew/procGen.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/mesh.h>
#include <iostream>
#include <vector>
#include <string>

#include "dh/heightMap.h"

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();
void initCamera();
void definePipeline();
void initDetails();
void calculateLightSpaceMatrices();
std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
void calculateCascadeSplits();
void loadSelectedHeightmap();
std::vector<float> blurHeightmapData(const std::vector<float>& data, int width, int height, int radius);

void renderHeightmap(ew::Shader shader, float time);
void shadowPass(ew::Shader shadowPass);
GLenum glCheckError_(const char* file, int line);

#define glCheckError() glCheckError_(__FILE__, __LINE__) 
#define MAX_CASCADES 8
const float M_PI = 3.14159265358979323846f;

// Global state
int screenWidth = 2160;
int screenHeight = 1440;
float prevFrameTime = 0.0f;
float deltaTime = 0.0f;

// Camera and controllers
ew::Camera camera;
ew::CameraController cameraController;

// Available heightmaps
struct HeightmapFile 
{
    std::string name;
    std::string path;
};

// Heightmap settings
struct HeightmapSettings 
{
    glm::vec3 scale = glm::vec3(100.0f, 20.0f, 100.0f);
    GLuint texture = 0;
    bool wireframe = false;
    float ambientStrength = 0.3f;
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f));
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 0.9f);
    int selectedHeightmap = 0;
    bool useColorMap = true;
    float waterLevel = 0.2f;
    glm::vec3 waterColor = glm::vec3(0.0f, 0.2f, 0.8f);
    glm::vec3 lowlandColor = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::vec3 highlandColor = glm::vec3(0.5f, 0.5f, 0.0f);
    glm::vec3 mountainColor = glm::vec3(0.5f, 0.5f, 0.5f);
    float shininess = 32.0f;
    float specularStrength = 0.2f;
    bool useBlur = false;
    int blurRadius = 1;
} heightmapSettings;

// Available heightmaps
std::vector<HeightmapFile> heightmapFiles = 
{
    {"North America", "assets/Textures/northamericaHeightMap.png"},
    {"Earth", "assets/Textures/earth.jpg"},
    {"Iceland", "assets/Textures/iceland_heightmap.png"},
    {"Breath Of The Wild", "assets/Textures/botw.jpg"}
};

// Current selected heightmap path
std::string currentHeightmapPath = "assets/Textures/northamericaHeightMap.png";

// Mesh and dimensions
ew::Mesh heightmapMesh;
int heightmapWidth = 0;
int heightmapHeight = 0;

// Shadow and lighting settings
struct Material 
{
    glm::vec3 ambient = glm::vec3(1.0);
    glm::vec3 diffuse = glm::vec3(0.5);
    glm::vec3 specular = glm::vec3(0.5);
    float shininess = 128;
} material;

struct Light 
{
    glm::vec3 color = glm::vec3(0.5f, 0.5f, 0.5f);
    glm::vec3 position = glm::vec3(1.0f);
    bool rotating = true;
} light;

struct Debug 
{
    glm::vec3 color = glm::vec3{ 0.00f, 0.31f, 0.85f };
    float bias = 0.005f;
    float max_bias = 0.05f;
    float min_bias = 0.005f;
    bool cull_front = true;
    bool use_pcf = true;
    bool visualize_cascades = true;
    int num_cascades = 3;
    bool enable_shadows = true;
} debug;

struct ViewFrustumSettings 
{
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float lambda = 0.5f;
} viewFrustum;

struct DepthBuffer 
{
    GLuint fbo;
    GLuint depthTexture;

    float width;
    float height;

    // Cascade specific data
    float cascadeSplits[MAX_CASCADES];       // Distances for cascade splits
    glm::mat4 lightViewProj[MAX_CASCADES];   // View-projection matrix for each cascade

    // Used for IMGUI visualization
    GLuint cascadeVisualizationTextures[MAX_CASCADES];

    void Initialize(float dWidth, float dHeight) 
    {
        width = dWidth;
        height = dHeight;

        // Create depth texture array
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, depthTexture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, dWidth, dHeight, MAX_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        // Texture parameters
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Gen framebuffer
        glGenFramebuffers(1, &fbo);

        // Init cascade splits values
        for (int i = 0; i < MAX_CASCADES; i++) 
        {
            // Spread evenly between 0 and 1
            cascadeSplits[i] = (i + 1.0f) / MAX_CASCADES;
        }

        // Separate textures for IMGUI visualization
        glGenTextures(MAX_CASCADES, cascadeVisualizationTextures);

        for (int i = 0; i < MAX_CASCADES; i++) 
        {
            glBindTexture(GL_TEXTURE_2D, cascadeVisualizationTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, dWidth, dHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        }
    }
} depthBuffer;

int main() 
{
    // Initialize window
    GLFWwindow* window = initWindow("Heightmap with Cascaded Shadow Maps", screenWidth, screenHeight);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // Load shaders
    ew::Shader heightmapShader = ew::Shader("assets/Shaders/heightmap.vert", "assets/Shaders/heightmap.frag");
    ew::Shader shadowPassShader = ew::Shader("assets/Shaders/shadow_pass.vert", "assets/Shaders/shadow_pass.frag");

    // Init camera and pipeline
    initCamera();
    definePipeline();
    initDetails();

    // Initialize shadow mapping
    depthBuffer.Initialize(screenWidth, screenHeight);

    // Load initial heightmap
    loadSelectedHeightmap();

    // Main loop
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        float time = (float)glfwGetTime();
        deltaTime = time - prevFrameTime;
        prevFrameTime = time;

        // Shadow pass (if enabled)
        if (debug.enable_shadows) 
        {
            shadowPass(shadowPassShader);
        }

        // Render scene with heightmap
        renderHeightmap(heightmapShader, time);

        // Camera movement
        cameraController.move(window, &camera, deltaTime);

        // Draw UI
        drawUI();

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteTextures(1, &heightmapSettings.texture);
    glDeleteTextures(MAX_CASCADES, depthBuffer.cascadeVisualizationTextures);
    glDeleteTextures(1, &depthBuffer.depthTexture);
    glDeleteFramebuffers(1, &depthBuffer.fbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) 
{
    // Reset position
    camera->position = glm::vec3(0, 50.0f, 100.0f);
    // Reset target
    camera->target = glm::vec3(0);

    // Reset controller rotation
    controller->yaw = -90.0f;
    controller->pitch = -25.0f;
}

void initCamera()
{
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);	//look at center of scene
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;
}

void initDetails()
{
	light.position = glm::vec3(1.0f);
	light.color = glm::vec3(0.5f, 0.5f, 0.5f);
	light.rotating = true;
}

void definePipeline() 
{
    // Define OpenGL pipeline
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
}

std::vector<float> blurHeightmapData(const std::vector<float>& data, int width, int height, int radius) 
{
    std::vector<float> result(data.size(), 0.0f);

    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            float sum = 0.0f;
            int count = 0;
            // Box blur kernel
            for (int ky = -radius; ky <= radius; ++ky) 
            {
                for (int kx = -radius; kx <= radius; ++kx) 
                {
                    int sx = x + kx;
                    int sy = y + ky;

                    // Check boundaries
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height) 
                    {
                        sum += data[sy * width + sx];
                        count++;
                    }
                }
            }

            // Average
            result[y * width + x] = sum / count;
        }
    }
    return result;
}

void loadSelectedHeightmap() 
{
    // Update the current heightmap path
    currentHeightmapPath = heightmapFiles[heightmapSettings.selectedHeightmap].path;
    std::printf("\n==== Loading heightmap: %s ====\n", currentHeightmapPath.c_str());

    // Clean up previous texture
    if (heightmapSettings.texture) 
    {
        glDeleteTextures(1, &heightmapSettings.texture);
        heightmapSettings.texture = 0;
    }

    // First get dimensions
    if (!dh::getHeightmapDimensions(currentHeightmapPath.c_str(), heightmapWidth, heightmapHeight)) 
    {
        std::printf("ERROR: Failed to get dimensions\n");
        return;
    }

    std::printf("Dimensions: %dx%d\n", heightmapWidth, heightmapHeight);

    // Check for extreme dimensions that might cause issues
    if (heightmapWidth > 4096 || heightmapHeight > 4096) 
    {
        std::printf("WARNING: Very large heightmap detected. Performance may be impacted.\n");
    }

    if (heightmapWidth < 16 || heightmapHeight < 16) 
    {
        std::printf("WARNING: Very small heightmap detected. Quality may be poor.\n");
    }

    // Load the height data
    std::vector<float> heightData = dh::loadHeightmapData(currentHeightmapPath.c_str(), true);

    // Verify data
    if (heightData.empty()) 
    {
        std::printf("ERROR: Failed to load height data\n");
        return;
    }

    if (heightData.size() != (size_t)(heightmapWidth * heightmapHeight)) 
    {
        std::printf("ERROR: Data size mismatch! Expected: %d, Got: %zu\n",
            heightmapWidth * heightmapHeight, heightData.size());
        return;
    }

    // Apply blur if needed
    if (heightmapSettings.useBlur && heightmapSettings.blurRadius > 0) 
    {
        std::printf("Applying blur with radius %d\n", heightmapSettings.blurRadius);
        heightData = blurHeightmapData(heightData, heightmapWidth, heightmapHeight, heightmapSettings.blurRadius);
    }

    // Create the mesh
    std::printf("Creating mesh...\n");
    heightmapMesh = dh::createHeightmapMesh(heightData, heightmapWidth, heightmapHeight, heightmapSettings.scale);

    // Load the texture for visualization
    std::printf("Loading texture...\n");
    heightmapSettings.texture = ew::loadTexture(currentHeightmapPath.c_str());

    if (heightmapSettings.texture == 0) 
    {
        std::printf("WARNING: Failed to load texture\n");
    }

    std::printf("Heightmap loaded successfully\n");
}

void renderHeightmap(ew::Shader shader, float time) 
{
    // Update light position if rotating
    if (light.rotating) 
    {
        const auto rotationMatrix = glm::rotate(time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        light.position = rotationMatrix * glm::vec4(100.0f, 100.0f, 100.0f, 1.0f);
    }

    const auto camera_view_proj = camera.projectionMatrix() * camera.viewMatrix();

    // Set viewport and clear buffers
    glViewport(0, 0, screenWidth, screenHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set wireframe mode if enabled
    if (heightmapSettings.wireframe) 
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else 
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    // Bind shadow map texture if shadows are enabled
    if (debug.enable_shadows) 
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, depthBuffer.depthTexture);
    }

    // Bind heightmap texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightmapSettings.texture);

    // Use shader and set uniforms
    shader.use();

    // Textures
    shader.setInt("_HeightmapTexture", 0);

    if (debug.enable_shadows) 
    {
        shader.setInt("shadow_map", 1);
    }

    // Shadow-specific uniforms if enabled
    if (debug.enable_shadows) 
    {
        shader.setInt("enable_shadows", 1);
        shader.setInt("cascade_count", debug.num_cascades);

        for (int i = 0; i < debug.num_cascades; i++) 
        {
            // Array of light view projection matrices
            shader.setMat4("_LightViewProjection[" + std::to_string(i) + "]", depthBuffer.lightViewProj[i]);
            // Cascade splits
            shader.setFloat("cascade_splits[" + std::to_string(i) + "]", depthBuffer.cascadeSplits[i]);
        }

        // Visualization flag
        shader.setInt("visualize_cascades", debug.visualize_cascades ? 1 : 0);
        shader.setFloat("far_clip_plane", viewFrustum.farPlane);

        // For shadowing
        shader.setFloat("bias", debug.bias);
        shader.setFloat("minBias", debug.min_bias);
        shader.setFloat("maxBias", debug.max_bias);
        shader.setInt("use_pcf", debug.use_pcf);
    }
    else 
    {
        shader.setInt("enable_shadows", 0);
    }

    // Basic scene matrices
    shader.setMat4("_Model", glm::mat4(1.0f));
    shader.setMat4("_ViewProjection", camera_view_proj);

    // Camera
    shader.setVec3("_CameraPos", camera.position);

    // Lighting
    shader.setVec3("_LightDir", -glm::normalize(light.position));
    shader.setVec3("_LightColor", light.color);
    shader.setVec3("_LightPos", light.position);
    shader.setFloat("_AmbientStrength", heightmapSettings.ambientStrength);
    shader.setFloat("_SpecularStrength", heightmapSettings.specularStrength);
    shader.setFloat("_Shininess", heightmapSettings.shininess);

    // Color mapping
    shader.setInt("_UseColorMap", heightmapSettings.useColorMap ? 1 : 0);
    shader.setFloat("_WaterLevel", heightmapSettings.waterLevel);
    shader.setVec3("_WaterColor", heightmapSettings.waterColor);
    shader.setVec3("_LowlandColor", heightmapSettings.lowlandColor);
    shader.setVec3("_HighlandColor", heightmapSettings.highlandColor);
    shader.setVec3("_MountainColor", heightmapSettings.mountainColor);

    // Draw the heightmap mesh
    heightmapMesh.draw();
}

void shadowPass(ew::Shader shadowPass) {
    // Calculate all light view-projection matrices for cascades
    calculateLightSpaceMatrices();

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Cull facing if needed
    if (debug.cull_front) 
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
    else 
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    // Render depth for each cascade
    for (unsigned int cascade = 0; cascade < debug.num_cascades; cascade++) 
    {
        // Bind framebuffer for this cascade
        glBindFramebuffer(GL_FRAMEBUFFER, depthBuffer.fbo);

        // Attach layer of depth texture array to this framebuffer depth attachment
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBuffer.depthTexture, 0, cascade);

        // Disable draw buffer
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) 
        {
            printf("Framebuffer incomplete: %d (cascade %d)\n", status, cascade);
        }

        // Clear depth buffer
        glViewport(0, 0, depthBuffer.width, depthBuffer.height);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Use shadow shader
        shadowPass.use();
        shadowPass.setMat4("_Model", glm::mat4(1.0f));
        shadowPass.setMat4("_LightViewProjection", depthBuffer.lightViewProj[cascade]);

        // Draw heightmap mesh
        heightmapMesh.draw();

        // After rendering copy the depth data to visualization texture for IMGUI
        glBindTexture(GL_TEXTURE_2D, depthBuffer.cascadeVisualizationTextures[cascade]);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, depthBuffer.width, depthBuffer.height);
    }

    // Reset framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Reset face culling
    glCullFace(GL_BACK);
}

void calculateLightSpaceMatrices() 
{
    calculateCascadeSplits();

    float lastSplitDist = 0.0f;

    // Calculate cascade split distances
    for (int i = 0; i < debug.num_cascades; i++) 
    {
        float splitDist = depthBuffer.cascadeSplits[i];

        // Frustum corners for this cascade
        glm::mat4 projectionMatrix = camera.projectionMatrix();
        glm::mat4 viewMatrix = camera.viewMatrix();

        // Cascade projection matrix with custom near/far planes
        float cascadeNear = lastSplitDist * (viewFrustum.farPlane - viewFrustum.nearPlane);
        float cascadeFar = splitDist * (viewFrustum.farPlane - viewFrustum.nearPlane);

        glm::mat4 cascadeProj = glm::perspective
        (
            glm::radians(camera.fov),
            camera.aspectRatio,
            viewFrustum.nearPlane + cascadeNear,
            viewFrustum.nearPlane + cascadeFar
        );

        std::vector<glm::vec4> frustumCorners = getFrustumCornersWorldSpace(cascadeProj, viewMatrix);

        // Center of frustum corners
        glm::vec3 center = glm::vec3(0.0f);
        for (const auto& corner : frustumCorners) 
        {
            center += glm::vec3(corner);
        }
        center /= frustumCorners.size();

        // Light view matrix looking at frustum center
        const glm::mat4 lightView = glm::lookAt
        (
            center + light.position,
            center,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        // Orthographic projection bounds to fit frustum
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        // Transform corners to light space and calc bounds
        for (const auto& corner : frustumCorners) 
        {
            const glm::vec4 lightSpaceCorner = lightView * corner;
            minX = std::min(minX, lightSpaceCorner.x);
            maxX = std::max(maxX, lightSpaceCorner.x);
            minY = std::min(minY, lightSpaceCorner.y);
            maxY = std::max(maxY, lightSpaceCorner.y);
            minZ = std::min(minZ, lightSpaceCorner.z);
            maxZ = std::max(maxZ, lightSpaceCorner.z);
        }

        // Padding to bound
        float radius = 20.0f;
        minX -= radius;
        maxX += radius;
        minY -= radius;
        maxY += radius;

        // Orthographic projection matrix
        const glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ - 100.0f, maxZ + 100.0f);

        // Final light view-projection matrix for this cascade
        depthBuffer.lightViewProj[i] = lightProj * lightView;

        lastSplitDist = splitDist;
    }
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
    // All 8 corners of frustum
    std::vector<glm::vec4> frustumCorners(8);
    const glm::mat4 inv = glm::inverse(proj * view);

    // 4 corners on near plane
    // 4 corners on far plane
    for (unsigned int x = 0; x < 2; ++x)            // Left or right side
    {                                       
        for (unsigned int y = 0; y < 2; ++y)        // Bottom or top side
        {                                   
            for (unsigned int z = 0; z < 2; ++z)    // Near side or far side
            {                               
                // Represent standard clip cube coords
                const glm::vec4 pt = inv * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f);

                // Get actual world pos
                frustumCorners[x * 4 + y * 2 + z] = pt / pt.w;
            }
        }
    }

    return frustumCorners;
}

void calculateCascadeSplits() 
{
    // Calculate ratio between far and near plane
    float ratio = viewFrustum.farPlane / viewFrustum.nearPlane;

    for (int i = 0; i < debug.num_cascades; i++)
    {
        // Normalized pos for this cascade
        float p = (i + 1) / static_cast<float>(debug.num_cascades);

        // Split calculation (for better precision)
        float log = viewFrustum.nearPlane * std::pow(ratio, p);

        // Uniform split
        float uniform = viewFrustum.nearPlane + (viewFrustum.farPlane - viewFrustum.nearPlane) * p;

        // Blend between logarithmic + uniform using lambda
        float d = viewFrustum.lambda * (log - uniform) + uniform;

        // Normalize so 0 = near plane and 1 = far plane
        depthBuffer.cascadeSplits[i] = (d - viewFrustum.nearPlane) / (viewFrustum.farPlane - viewFrustum.nearPlane);
    }
}

GLenum glCheckError_(const char* file, int line) 
{
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) 
    {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}

void drawUI() 
{
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Terrain Renderer Controls");

    // Camera reset
    if (ImGui::Button("Reset Camera")) 
    {
        resetCamera(&camera, &cameraController);
    }

    // Heightmap selection
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Heightmap Selection")) 
    {
        // Create array of names for combo box
        std::vector<const char*> heightmapNames;
        for (const auto& hm : heightmapFiles) 
        {
            heightmapNames.push_back(hm.name.c_str());
        }

        // Heightmap selection combo box
        if (ImGui::Combo("Select Heightmap", &heightmapSettings.selectedHeightmap,
            heightmapNames.data(), heightmapNames.size())) 
        {
            loadSelectedHeightmap();
        }

        // Scale controls
        ImGui::Text("Heightmap Scale");
        ImGui::SliderFloat("X Scale", &heightmapSettings.scale.x, 10.0f, 200.0f);
        ImGui::SliderFloat("Y Scale (Height)", &heightmapSettings.scale.y, 1.0f, 50.0f);
        ImGui::SliderFloat("Z Scale", &heightmapSettings.scale.z, 10.0f, 200.0f);

        // Rendering options
        ImGui::Text("Rendering");
        ImGui::Checkbox("Wireframe", &heightmapSettings.wireframe);
        ImGui::Checkbox("Use Color Map", &heightmapSettings.useColorMap);

        // Blur settings
        ImGui::Text("Blur Settings");
        bool blurChanged = false;
        blurChanged |= ImGui::Checkbox("Use Blur", &heightmapSettings.useBlur);
        blurChanged |= ImGui::SliderInt("Blur Radius", &heightmapSettings.blurRadius, 0, 10);

        // Regenerate mesh if blur settings change
        if (blurChanged) 
        {
            loadSelectedHeightmap();
        }

        // Color mapping
        if (heightmapSettings.useColorMap) 
        {
            ImGui::SliderFloat("Water Level", &heightmapSettings.waterLevel, 0.0f, 0.5f);
            ImGui::ColorEdit3("Water Color", &heightmapSettings.waterColor.x);
            ImGui::ColorEdit3("Lowland Color", &heightmapSettings.lowlandColor.x);
            ImGui::ColorEdit3("Highland Color", &heightmapSettings.highlandColor.x);
            ImGui::ColorEdit3("Mountain Color", &heightmapSettings.mountainColor.x);
        }

        if (ImGui::Button("Regenerate Mesh")) 
        {
            loadSelectedHeightmap();
        }
        ImGui::Text("Heightmap: %dx%d", heightmapWidth, heightmapHeight);
    }

    // Material settings
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Material")) 
    {
        ImGui::SliderFloat3("Ambient K", (float*)&material.ambient, 0.0f, 1.0f);
        ImGui::SliderFloat3("Diffuse K", (float*)&material.diffuse, 0.0f, 1.0f);
        ImGui::SliderFloat3("Specular K", (float*)&material.specular, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &material.shininess, 2.0f, 1024.0f);

        // Basic lighting from heightmap settings
        ImGui::Separator();
        ImGui::Text("Basic Lighting");
        ImGui::SliderFloat("Ambient", &heightmapSettings.ambientStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &heightmapSettings.specularStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Material Shininess", &heightmapSettings.shininess, 1.0f, 128.0f);
    }

    // View frustrum settings
    ImGui::Separator();

    if (ImGui::CollapsingHeader("View Frustrum Settings")) 
    {
        //store to detect changes
        float prevNear = viewFrustum.nearPlane;
        float prevFar = viewFrustum.farPlane;
        float prevLambda = viewFrustum.lambda;
        //near plane
        ImGui::SliderFloat("Near Plane", &viewFrustum.nearPlane, 0.01f, 10.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##Near")) viewFrustum.nearPlane = 0.1f;
        //far plane
        ImGui::SliderFloat("Far Plane", &viewFrustum.farPlane, 10.0f, 1000.0f, "%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##Far")) viewFrustum.farPlane = 100.0f;
        //lambda
        ImGui::SliderFloat("Lambda", &viewFrustum.lambda, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset##Lambda")) viewFrustum.lambda = 0.75f;

        //recalc cascade splits if any values changed
        if (prevNear != viewFrustum.nearPlane ||
            prevFar != viewFrustum.farPlane ||
            prevLambda != viewFrustum.lambda) 
        {
            calculateCascadeSplits();
        }
    }

    // Shadow settings
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Shadow Pass")) 
    {
        ImGui::SliderFloat("Bias", &debug.bias, 0.0f, 0.1f);
        ImGui::SliderFloat("Min Bias", &debug.min_bias, 0.0f, 0.1f);
        ImGui::SliderFloat("Max Bias", &debug.max_bias, 0.0f, 0.1f);
        ImGui::Checkbox("Culling Front", &debug.cull_front);
        ImGui::Checkbox("Using PCF", &debug.use_pcf);
    }

    // Cascade settings
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Cascade Settings")) 
    {
        ImGui::Checkbox("Show Cascade Colors", &debug.visualize_cascades);
        //control for number of cascades
        int prevCascades = debug.num_cascades;
        ImGui::SliderInt("Number of Cascades", &debug.num_cascades, 1, MAX_CASCADES);

        if (prevCascades != debug.num_cascades) 
        {
            //recalc cascade splits
            calculateCascadeSplits();
        }

        //depth image
        ImGui::Separator();
        for (int i = 0; i < debug.num_cascades; i++) 
        {
            ImGui::Text("Cascade %d:", i);
            ImGui::Image((ImTextureID)(intptr_t)depthBuffer.cascadeVisualizationTextures[i], ImVec2(256, 256));
        }
    }

    // Lighting settings
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Lighting")) 
    {
        ImGui::ColorEdit3("Light Color", &light.color[0]);
        ImGui::Checkbox("Light Rotation?", &light.rotating);
        ImGui::SliderFloat("X Position", &light.position.x, -5, 5);
        ImGui::SliderFloat("Y Position", &light.position.y, -5, 5);
        ImGui::SliderFloat("Z Position", &light.position.z, -5, 5);

        // Directional light settings from heightmap
        ImGui::Separator();
        ImGui::Text("Directional Light");
        ImGui::ColorEdit3("Direction Light Color", &heightmapSettings.lightColor.x);

        float lightDirAngles[2] = 
        {
            atan2(heightmapSettings.lightDir.x, heightmapSettings.lightDir.z) * 180.0f / M_PI,
            asin(-heightmapSettings.lightDir.y) * 180.0f / M_PI
        };

        if (ImGui::SliderFloat2("Light Direction", lightDirAngles, -180.0f, 180.0f)) 
        {
            float horz = lightDirAngles[0] * M_PI / 180.0f;
            float vert = lightDirAngles[1] * M_PI / 180.0f;
            heightmapSettings.lightDir.x = cos(vert) * sin(horz);
            heightmapSettings.lightDir.y = -sin(vert);
            heightmapSettings.lightDir.z = cos(vert) * cos(horz);
            heightmapSettings.lightDir = glm::normalize(heightmapSettings.lightDir);
        }
    }

    // Performance stats
    ImGui::Separator();
    ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
    ImGui::Text("FPS: %.1f", 1.0f / deltaTime);

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    screenWidth = width;
    screenHeight = height;
    camera.aspectRatio = (float)screenWidth / screenHeight;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
    printf("Initializing...");
    if (!glfwInit()) {
        printf("GLFW failed to init!");
        return nullptr;
    }

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL) {
        printf("GLFW failed to create window");
        return nullptr;
    }
    //whenever you init you need to send current context
    glfwMakeContextCurrent(window);

    //when we load the proc address, computer if figuring out where these function calls live on the GPU
    if (!gladLoadGL(glfwGetProcAddress)) {
        printf("GLAD Failed to load GL headers");
        return nullptr;
    }

    //Initialize ImGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    return window;
}