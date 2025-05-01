#include <stdio.h>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/transform.hpp"
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
#include <iostream>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();
void initCamera();
void definePipline();
void initDetails();
void calculateLightSpaceMatrices();
std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
void calculateCascadeSplits(float nearPlane, float farPlane);

void render(ew::Shader shader, ew::Model model, GLuint texture, float time);
void shadowPass(ew::Shader shadowPass, ew::Model model);
GLenum glCheckError_(const char* file, int line)
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR)
	{
		std::string error;
		switch (errorCode)
		{
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
#define glCheckError() glCheckError_(__FILE__, __LINE__) 

// Global state
int screenWidth = 2160;
int screenHeight = 1440;
float prevFrameTime;
float deltaTime;

// Add these definitions
#define MAX_CASCADES 3
#define NUM_CASCADES 3

ew::Camera camera;	//our camera
ew::CameraController cameraController;
ew::Transform monkeyTransform;
ew::Mesh plane;
static glm::vec4 light_orbit_radius = { 2.0f, 2.0f, -2.0f, 1.0f };

struct Material
{
	glm::vec3 ambient = glm::vec3(1.0);
	glm::vec3 diffuse = glm::vec3(0.5);
	glm::vec3 specular = glm::vec3(0.5);
	float shininess = 128;
}material;

struct Light
{
	glm::vec3 color;
	glm::vec3 position;
	bool rotating;
}light;

struct Debug
{
	glm::vec3 color = glm::vec3{ 0.00f, 0.31f, 0.85f };
	float bias = 0.005f;
	float max_bias = 0.05;
	float min_bias = 0.005;
	bool cull_front = true;
	bool use_pcf = true;
	bool visualize_cascades = true; // New flag to toggle cascade visualization
}debug;

struct DepthBuffer
{
	GLuint fbo;
	GLuint depthTexture;

	float width;
	float height;

	// Cascade-specific data
	float cascadeSplits[MAX_CASCADES];  // Normalized distances for cascade splits
	glm::mat4 lightViewProj[MAX_CASCADES]; // View-projection matrix for each cascade

	void Initialize(float dWidth, float dHeight)
	{
		width = dWidth;
		height = dHeight;

		// Create depth texture array
		glGenTextures(1, &depthTexture);
		glBindTexture(GL_TEXTURE_2D_ARRAY, depthTexture);
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_DEPTH_COMPONENT32F,
			dWidth,
			dHeight,
			MAX_CASCADES,
			0,
			GL_DEPTH_COMPONENT,
			GL_FLOAT,
			nullptr);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

		// Create framebuffer
		glGenFramebuffers(1, &fbo);

		// Setup default cascade splits (can be adjusted later)
		cascadeSplits[0] = 0.05f;  // First cascade covers very close objects (5%)
		cascadeSplits[1] = 0.25f;  // Second cascade extends to medium distance (25%)
		cascadeSplits[2] = 1.0f;   // Third cascade covers the rest
	}
} depthBuffer;

int main()
{
	GLFWwindow* window = initWindow("Cascaded Shadow Maps", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	//shader
	ew::Shader blinnPhongShader = ew::Shader("assets/bp.vert", "assets/bp.frag");
	ew::Shader shadow_pass = ew::Shader("assets/shadow_pass.vert", "assets/shadow_pass.frag");

	//model + texture
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");

	//init stuff
	initCamera();
	definePipline();
	initDetails();

	depthBuffer.Initialize(screenWidth, screenHeight);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		shadowPass(shadow_pass, monkeyModel);

		render(blinnPhongShader, monkeyModel, brickTexture, time);

		cameraController.move(window, &camera, deltaTime);

		drawUI();

		glfwSwapBuffers(window);
	}
	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller)
{
	//reset position
	camera->position = glm::vec3(0, 0, 5.0f);
	//reset target
	camera->target = glm::vec3(0);

	//reset controller rotation
	controller->yaw = controller->pitch = 0;
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
	plane.load(ew::createPlane(50.0f, 50.0f, 100));
	light.position = glm::vec3(1.0f);
	light.color = glm::vec3(0.5f, 0.5f, 0.5f);
	light.rotating = true;
}

void render(ew::Shader shader, ew::Model model, GLuint texture, float time)
{
	if (light.rotating)
	{
		const auto rym = glm::rotate((float)time, glm::vec3(0.0f, 1.0f, 0.0f));
		light.position = rym * light_orbit_radius;
	}

	const auto camera_view_proj = camera.projectionMatrix() * camera.viewMatrix();

	//render lighting
	glViewport(0, 0, screenWidth, screenHeight);

	glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depthBuffer.depthTexture);

	shader.use();

	//samplers
	shader.setInt("shadow_map", 0);

	//scene matrices
	shader.setMat4("_Model", glm::mat4(1.0f));
	shader.setMat4("_CameraViewProjection", camera_view_proj);

	// Pass cascade-specific data
	shader.setInt("cascade_count", NUM_CASCADES);
	for (int i = 0; i < NUM_CASCADES; i++) {
		// Pass array of light view projection matrices
		shader.setMat4("_LightViewProjection[" + std::to_string(i) + "]", depthBuffer.lightViewProj[i]);
		// Pass cascade splits
		shader.setFloat("cascade_splits[" + std::to_string(i) + "]", depthBuffer.cascadeSplits[i]);
	}

	// Pass visualization flag
	shader.setInt("visualize_cascades", debug.visualize_cascades ? 1 : 0);

	shader.setVec3("camera_pos", camera.position);

	//material properties
	shader.setVec3("_Material.ambient", material.ambient);
	shader.setVec3("_Material.diffuse", material.diffuse);
	shader.setVec3("_Material.specular", material.specular);
	shader.setFloat("_Material.shininess", material.shininess);

	//point light
	shader.setVec3("_Light.color", light.color);
	shader.setVec3("_Light.position", light.position);

	//for shadowing
	shader.setFloat("bias", debug.bias);
	shader.setFloat("minBias", debug.min_bias);
	shader.setFloat("maxBias", debug.max_bias);
	shader.setInt("use_pcf", debug.use_pcf);

	model.draw();

	shader.setMat4("_Model", glm::translate(glm::vec3(0.0f, -2.0f, 0.0f)));

	plane.draw();
}

void shadowPass(ew::Shader shadowPass, ew::Model model)
{
	// Calculate all light view-projection matrices for cascades
	// This is called every frame, so splits will update as camera moves
	calculateLightSpaceMatrices();

	// Enable depth testing
	glEnable(GL_DEPTH_TEST);

	// Set up face culling if needed
	if (debug.cull_front) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	else {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}

	// Render depth for each cascade
	for (unsigned int cascade = 0; cascade < NUM_CASCADES; cascade++)
	{
		// Bind the framebuffer for this cascade
		glBindFramebuffer(GL_FRAMEBUFFER, depthBuffer.fbo);

		// Attach the appropriate layer of depth texture array to the framebuffer's depth attachment
		glFramebufferTextureLayer(
			GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			depthBuffer.depthTexture,
			0,  // mip level
			cascade
		);

		// Disable draw buffer since we're only writing to depth
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		// Check framebuffer status
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("Framebuffer incomplete: %d (cascade %d)\n", status, cascade);
		}

		// Clear the depth buffer
		glViewport(0, 0, depthBuffer.width, depthBuffer.height);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Use shadow shader
		shadowPass.use();
		shadowPass.setMat4("_Model", glm::mat4(1.0f));
		shadowPass.setMat4("_LightViewProjection", depthBuffer.lightViewProj[cascade]);

		// Draw the model
		model.draw();

		// Draw the plane too
		shadowPass.setMat4("_Model", glm::translate(glm::vec3(0.0f, -2.0f, 0.0f)));
		plane.draw();
	}

	// Reset to default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Reset face culling
	glCullFace(GL_BACK);
}

void calculateLightSpaceMatrices()
{
	const float nearClip = 0.1f;
	const float farClip = 100.0f;
	calculateCascadeSplits(nearClip, farClip);

	float lastSplitDist = 0.0f;

	// Calculate split distances in world-space units
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float splitDist = depthBuffer.cascadeSplits[i];

		// Get frustum corners for this cascade segment
		glm::mat4 projectionMatrix = camera.projectionMatrix();
		glm::mat4 viewMatrix = camera.viewMatrix();

		// Create cascade projection matrix with custom near/far planes
		float cascadeNear = lastSplitDist * (farClip - nearClip);
		float cascadeFar = splitDist * (farClip - nearClip);

		glm::mat4 cascadeProj = glm::perspective(
			glm::radians(camera.fov),
			camera.aspectRatio,
			nearClip + cascadeNear,
			nearClip + cascadeFar
		);

		std::vector<glm::vec4> frustumCorners = getFrustumCornersWorldSpace(cascadeProj, viewMatrix);

		// Calculate the center of frustum corners
		glm::vec3 center = glm::vec3(0.0f);
		for (const auto& corner : frustumCorners)
		{
			center += glm::vec3(corner);
		}
		center /= frustumCorners.size();

		// tried using light direction but it did not work :(
		// glm::vec3 lightDir = glm::normalize(glm::vec3(light.position - center));

		// Calculate light view matrix looking at frustum center
		const glm::mat4 lightView = glm::lookAt(
			center + light.position,
			center,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		// Calculate orthographic projection bounds to fit frustum slice
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();

		// Transform corners to light space and calculate bounds
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

		// Add padding to bound
		float radius = 5.0f;
		minX -= radius;
		maxX += radius;
		minY -= radius;
		maxY += radius;

		// Create orthographic projection matrix for this cascade
		const glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ - 100.0f, maxZ + 100.0f); //why are the 100s here

		// Calculate and store the final light view-projection matrix for this cascade
		depthBuffer.lightViewProj[i] = lightProj * lightView;

		lastSplitDist = splitDist;
	}
}

// Add this function to calculate frustum corners in world space
std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
	std::vector<glm::vec4> frustumCorners(8);
	const glm::mat4 inv = glm::inverse(proj * view);

	// Near and far planes for NDC coordinates
	for (unsigned int x = 0; x < 2; ++x)
	{
		for (unsigned int y = 0; y < 2; ++y)
		{
			for (unsigned int z = 0; z < 2; ++z)
			{
				const glm::vec4 pt = inv * glm::vec4(
					2.0f * x - 1.0f,
					2.0f * y - 1.0f,
					2.0f * z - 1.0f,
					1.0f);
				frustumCorners[x * 4 + y * 2 + z] = pt / pt.w;
			}
		}
	}

	return frustumCorners;
}

// Add this function to calculate the cascade splits using a practical split scheme
void calculateCascadeSplits(float nearPlane, float farPlane)
{
	float lambda = 0.5f;
	float ratio = farPlane / nearPlane;

	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float p = (i + 1) / static_cast<float>(NUM_CASCADES);
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + (farPlane - nearPlane) * p;
		float d = lambda * (log - uniform) + uniform;
		depthBuffer.cascadeSplits[i] = (d - nearPlane) / (farPlane - nearPlane);
	}
}

void definePipline()
{
	//pipeline definition
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	if (ImGui::Button("Reset Camera"))
	{
		resetCamera(&camera, &cameraController);
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Material"))
	{
		ImGui::SliderFloat3("Ambient K", (float*)&material.ambient, 0.0f, 1.0f);
		ImGui::SliderFloat3("Diffuse K", (float*)&material.diffuse, 0.0f, 1.0f);
		ImGui::SliderFloat3("Specular K", (float*)&material.specular, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.shininess, 2.0f, 1024.0f);
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Shadow Pass"))
	{
		ImGui::SliderFloat("Bias", &debug.bias, 0.0f, 0.1f);
		ImGui::SliderFloat("Min Bias", &debug.min_bias, 0.0f, 0.1f);
		ImGui::SliderFloat("Max Bias", &debug.max_bias, 0.0f, 0.1f);
		ImGui::Checkbox("Culling Front", &debug.cull_front);
		ImGui::Checkbox("Using PCF", &debug.use_pcf);
		ImGui::Checkbox("Visualize Cascades", &debug.visualize_cascades);

		ImGui::Text("Cascade Splits:");
		ImGui::SliderFloat("Split 1", &depthBuffer.cascadeSplits[0], 0.05f, 0.3f);
		ImGui::SliderFloat("Split 2", &depthBuffer.cascadeSplits[1], 0.3f, 0.7f);

		ImGui::Separator(); //depth image
		ImGui::Text("Cascade 0:");
		ImGui::Image((ImTextureID)(intptr_t)depthBuffer.depthTexture, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(), 0); // Layer 0

		ImGui::Text("Cascade 1:");
		ImGui::Image((ImTextureID)(intptr_t)depthBuffer.depthTexture, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(), 1); // Layer 1

		ImGui::Text("Cascade 2:");
		ImGui::Image((ImTextureID)(intptr_t)depthBuffer.depthTexture, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(), 2); // Layer 2
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::ColorEdit3("Color", &light.color[0]);
		ImGui::Checkbox("Light Rotation?", &light.rotating);
		ImGui::SliderFloat("X Position", &light.position.x, -5, 5);
		ImGui::SliderFloat("Y Position", &light.position.y, -5, 5);
		ImGui::SliderFloat("Z Position", &light.position.z, -5, 5);
	}
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