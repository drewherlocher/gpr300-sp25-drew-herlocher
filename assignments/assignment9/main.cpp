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
#include <ew/external/stb_image.h>

// Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime = 0.0f;
float deltaTime = 0.0f;

// Forward declarations
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

// Camera and transforms
ew::Camera camera;
ew::CameraController cameraController;

// Heightmap data
struct HeightmapData {
    GLuint VAO, VBO, EBO;
    unsigned int width, height;
    unsigned int numStrips;
    unsigned int numVertsPerStrip;
    GLuint texture;
};

HeightmapData heightmap;

// Helper Functions
void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
    camera->position = glm::vec3(0, 50.0f, 50.0f);
    camera->target = glm::vec3(0);
    controller->yaw = controller->pitch = 0;
}

void drawUI() {
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Controls");

    if (ImGui::Button("Reset Camera")) {
        resetCamera(&camera, &cameraController);
    }

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

HeightmapData createHeightMap() {
    HeightmapData data;

    int width, height, nChannels;
    unsigned char* imageData = stbi_load("assets/northamericaHeightMap.png",
        &width, &height, &nChannels, 0);

    if (!imageData) {
        printf("Failed to load heightmap image!\n");
        exit(-1);
    }

    data.width = width;
    data.height = height;
    data.numStrips = height - 1;
    data.numVertsPerStrip = width * 2;

    // Store heightmap as texture for visualization
    data.texture = ew::loadTexture("assets/northamericaHeightMap.png");

    std::vector<float> vertices;
    float yScale = 0.25, yShift = 16.0f;  // apply a scale+shift to the height data

    // Generate vertices
    for (unsigned int i = 0; i < height; i++) {
        for (unsigned int j = 0; j < width; j++) {
            // retrieve texel for (i,j) tex coord
            unsigned char* texel = imageData + (j + width * i) * nChannels;
            // raw height at coordinate
            unsigned char y = texel[0];

            // vertex
            vertices.push_back(-height / 2.0f + i);      // v.x
            vertices.push_back((int)y * yScale - yShift); // v.y
            vertices.push_back(-width / 2.0f + j);       // v.z

            // Add texture coordinates
            vertices.push_back(j / (float)(width - 1));  // u
            vertices.push_back(i / (float)(height - 1)); // v
        }
    }

    // Generate indices for triangle strips
    std::vector<unsigned int> indices;
    for (unsigned int i = 0; i < height - 1; i++) {
        for (unsigned int j = 0; j < width; j++) {
            for (unsigned int k = 0; k < 2; k++) {
                indices.push_back(j + width * (i + k));
            }
        }
    }

    // Setup VAO, VBO, EBO
    glGenVertexArrays(1, &data.VAO);
    glBindVertexArray(data.VAO);

    glGenBuffers(1, &data.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, data.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    // Position attribute (3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &data.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    stbi_image_free(imageData);
    return data;
}

void renderHeightmap(ew::Shader& shader, HeightmapData& heightmap) {
    shader.use();

    glBindVertexArray(heightmap.VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightmap.texture);

    // Render the mesh triangle strip by triangle strip - each row at a time
    for (unsigned int strip = 0; strip < heightmap.numStrips; ++strip) {
        glDrawElements(
            GL_TRIANGLE_STRIP,
            heightmap.numVertsPerStrip,
            GL_UNSIGNED_INT,
            (void*)(sizeof(unsigned int) * heightmap.numVertsPerStrip * strip)
        );
    }
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

    // Create the heightmap
    heightmap = createHeightMap();

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

        // Set shader uniforms
        heightmapShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
        heightmapShader.setMat4("_Model", glm::mat4(1.0f)); // Identity matrix
        heightmapShader.setInt("_HeightmapTexture", 0);

        // Render the heightmap
        renderHeightmap(heightmapShader, heightmap);

        // Draw UI
        drawUI();

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteVertexArrays(1, &heightmap.VAO);
    glDeleteBuffers(1, &heightmap.VBO);
    glDeleteBuffers(1, &heightmap.EBO);
    glDeleteTextures(1, &heightmap.texture);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}