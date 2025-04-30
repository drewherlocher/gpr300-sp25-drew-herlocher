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


void render(ew::Shader shader, ew::Model model, GLuint texture, float time);
void shadowPass(ew::Shader shadowPass, ew::Model model);
void calculateCascadeSplits();
std::vector<glm::mat4> calculateLightSpaceMatrices();
glm::mat4 calculateLightSpaceWithTexelSnapping(glm::mat4 lightView, glm::vec3 center, float texelSize);
std::vector<glm::vec4> calculateFrustumCorners(float nearSplit, float farSplit);

void reinitializeDepthBuffer();
void resetCamera(ew::Camera* camera, ew::CameraController* controller);
void initCamera();
void initDetails();
void definePipline();
void drawUI();
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
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

//global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;
const int MAX_CASCADES = 6;

ew::Camera camera;	//our camera
ew::CameraController cameraController;
ew::Transform monkeyTransform;
ew::Mesh plane;

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
	bool rotating = false;
}light;

struct Debug
{
	glm::vec3 color = glm::vec3{ 0.00f, 0.31f, 0.85f };
	float bias = 0.005f;
	float max_bias = 0.05;
	float min_bias = 0.005;
	bool cull_front = true;
	bool use_pcf = true;
	bool show_cascades = true;
	int cascade_to_view = 0;
	int num_cascades = 3;
}debug;

struct ViewFrustumSettings
{
	float nearPlane = 0.1f;
	float farPlane = 100.0f;
	float lambda = 0.75f;
}viewFrustum;

struct DepthBuffer
{
	GLuint fbo;
	GLuint depthTextures[MAX_CASCADES];

	float width;
	float height;

	void Initialize(float dWidth, float dHeight)
	{
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		{
			width = dWidth;
			height = dHeight;

			//depth attachment
			glGenTextures(MAX_CASCADES, depthTextures);

			//for each cascade
			for (int i = 0; i < MAX_CASCADES; i++) 
			{
				glBindTexture(GL_TEXTURE_2D, depthTextures[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, dWidth, dHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
			}

			//attach first texture for initial completeness check
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTextures[0], 0);

			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				printf("Framebuffer incomplete: %d", fboStatus);
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Cleanup() const
	{
		glDeleteTextures(MAX_CASCADES, depthTextures);
		glDeleteFramebuffers(1, &fbo);
	}

}depthBuffer;

//CSM constants
float cascadeSplits[MAX_CASCADES];
std::vector<glm::mat4> lightSpaceMatrices;
static glm::vec4 light_orbit_radius = { 2.0f, 2.0f, -2.0f, 1.0f };

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

	//init cascades
	calculateCascadeSplits();

	//init depth buffer
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

	depthBuffer.Cleanup();

	printf("Shutting down...");
}

void render(ew::Shader shader, ew::Model model, GLuint texture, float time)
{
	if (light.rotating)
	{
		const auto rym = glm::rotate(floor((float)time * 60.0f) / 60.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		//const auto rym = glm::rotate((float)time, glm::vec3(0.0f, 1.0f, 0.0f));
		light.position = rym * light_orbit_radius;
	}

	const auto camera_view_proj = camera.projectionMatrix() * camera.viewMatrix();

	//update light space matrices
	lightSpaceMatrices = calculateLightSpaceMatrices(); 

	//render lighting
	glViewport(0, 0, screenWidth, screenHeight);

	glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	// Activate texture units for shadow maps
	for (int i = 0; i < debug.num_cascades; i++) 
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, depthBuffer.depthTextures[i]);
	}

	shader.use();

	//shadow map samplers
	for (int i = 0; i < debug.num_cascades; i++) 
	{
		shader.setInt("shadow_maps[" + std::to_string(i) + "]", i);
	}

	//cascade splits
	for (int i = 0; i < debug.num_cascades; i++) 
	{
		shader.setFloat("cascade_splits[" + std::to_string(i) + "]", cascadeSplits[i]);
	}

	//light space matrices
	for (int i = 0; i < debug.num_cascades; i++) 
	{
		shader.setMat4("light_space_matrices[" + std::to_string(i) + "]", lightSpaceMatrices[i]);
	}

	//num cascades
	shader.setInt("NUM_CASCADES", debug.num_cascades); 

	//scene matrices
	shader.setMat4("_Model", glm::mat4(1.0f));
	shader.setMat4("_CameraViewProjection", camera_view_proj);
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
	shader.setFloat("Min Bias", debug.min_bias);
	shader.setFloat("Max Bias", debug.max_bias);
	shader.setInt("use_pcf", debug.use_pcf);
	shader.setInt("show_cascades", debug.show_cascades);

	//close monkey
	shader.setMat4("_Model", glm::translate(glm::vec3(0.0f, -3.0f, 0.0f)));
	model.draw();

	//far monkey
	shader.setMat4("_Model", glm::translate(glm::vec3(0.0f, -3.0f, -20.0f)));
	model.draw();

	shader.setMat4("_Model", glm::translate(glm::vec3(0.0f, -4.0f, 0.0f)));
	plane.draw();
}

void shadowPass(ew::Shader shadowPass, ew::Model model)
{
	//shadow pass
	glBindFramebuffer(GL_FRAMEBUFFER, depthBuffer.fbo);
	{
		//update light space matrices
		lightSpaceMatrices = calculateLightSpaceMatrices(); 

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		if (debug.cull_front) 
		{ 
			glCullFace(GL_FRONT); 
		}
		else 
		{
			glCullFace(GL_BACK); 
		}

		//render shadow map for each cascade
		for (int i = 0; i < debug.num_cascades; i++) 
		{
			//attach texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthBuffer.depthTextures[i], 0);

			glViewport(0, 0, depthBuffer.width, depthBuffer.height);
			glClear(GL_DEPTH_BUFFER_BIT);

			shadowPass.use();
			shadowPass.setMat4("_LightViewProjection", lightSpaceMatrices[i]);

			//close monkey
			shadowPass.setMat4("_Model", glm::translate(glm::vec3(0.0f, -3.0f, 0.0f)));
			model.draw();
			
			//far monkey
			shadowPass.setMat4("_Model", glm::translate(glm::vec3(0.0f, -3.0f, -20.0f)));
			model.draw();

			//also render plane
			//shadowPass.setMat4("_Model", glm::translate(glm::vec3(0.0f, -4.0f, 0.0f)));
		}

		glCullFace(GL_BACK);
		glCheckError();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void calculateCascadeSplits()
{
	//used for distribution
	float ratio = viewFrustum.farPlane / viewFrustum.nearPlane;

	//clear array
	for (int i = 0; i < MAX_CASCADES; i++)
	{
		cascadeSplits[i] = 0.0f;
	}

	//for each cascade
	for (int i = 0; i < debug.num_cascades; i++)
	{
		//calc normalized position
		float p = (i + 1) / float(debug.num_cascades);

		//calc uniform split distance
		float uniform = viewFrustum.nearPlane + (viewFrustum.farPlane - viewFrustum.nearPlane) * p;

		//calc logarithmic split distance
		float logarithmic = viewFrustum.nearPlane * std::pow(ratio, p);

		//mix uniform + logarithmic based on Lambda
		cascadeSplits[i] = viewFrustum.lambda * logarithmic + (1 - viewFrustum.lambda) * uniform;
	}
}

std::vector<glm::mat4> calculateLightSpaceMatrices()
{
	std::vector<glm::mat4> matrices(debug.num_cascades);

	//calculate light space matrix for each cascade
	for (int i = 0; i < debug.num_cascades; i++) 
	{
		float nearSplit = (i == 0) ? 0.1f : cascadeSplits[i - 1];
		float farSplit = cascadeSplits[i];

		//frustum corners in view space
		std::vector<glm::vec4> corners = calculateFrustumCorners(nearSplit, farSplit);

		//view space -> world space
		glm::mat4 invView = glm::inverse(camera.viewMatrix());
		for (auto& corner : corners) 
		{
			corner = invView * corner;
		}

		//center of the frustrum
		glm::vec3 center(0.0f);
		for (const auto& corner : corners) 
		{
			center += glm::vec3(corner);
		}
		center /= corners.size();

		//light view matrix LOOKING AT frustum center
		glm::mat4 lightView = glm::lookAt
		(
			light.position,
			center,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);

		//calc bounding box
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();

		for (const auto& corner : corners) 
		{
			glm::vec4 lightCorner = lightView * corner;
			minX = std::min(minX, lightCorner.x);
			maxX = std::max(maxX, lightCorner.x);
			minY = std::min(minY, lightCorner.y);
			maxY = std::max(maxY, lightCorner.y);
			minZ = std::min(minZ, lightCorner.z);
			maxZ = std::max(maxZ, lightCorner.z);
		}

		//calc dimensions
		float width = maxX - minX;
		float height = maxY - minY;

		//calc texel size
		float texelSize = width / depthBuffer.width;

		//THIS IS WHERE WE CALL THE TEXEL SNAPPING FUNCTION
		lightView = calculateLightSpaceWithTexelSnapping(lightView, center, texelSize);

		//add padding to bounding box
		//const float zMultiplier = 10.0f;
		//minZ *= zMultiplier;
		//maxZ *= zMultiplier;

		constexpr float zMult = 10.0f;
		if (minZ < 0)
		{
			minZ *= zMult;
		}
		else
		{
			minZ /= zMult;
		}
		if (maxZ < 0)
		{
			maxZ /= zMult;
		}
		else
		{
			maxZ *= zMult;
		}

		//orthographic projection
		glm::mat4 lightProjection = glm::ortho
		(
			minX, maxX,
			minY, maxY,
			maxZ, minZ
		);

		matrices[i] = lightProjection * lightView;
	}

	return matrices;
}

glm::mat4 calculateLightSpaceWithTexelSnapping(glm::mat4 lightView, glm::vec3 center, float texelSize)
{
	//world-space frustum center -> light space
	glm::vec3 lightSpaceCenter = glm::vec3(lightView * glm::vec4(center, 1.0f));

	//nearest texel -> stabilize shadows
	lightSpaceCenter.x = std::floor(lightSpaceCenter.x / texelSize) * texelSize;
	lightSpaceCenter.y = std::floor(lightSpaceCenter.y / texelSize) * texelSize;

	//mmove to the snapped position
	glm::mat4 texelSnappedTranslation = glm::translate(glm::mat4(1.0f), -lightSpaceCenter);

	// return matrix
	return texelSnappedTranslation * lightView;
}

std::vector<glm::vec4> calculateFrustumCorners(float nearSplit, float farSplit) 
{
	std::vector<glm::vec4> corners(8);

	//view space frustrum dimensions
	const float aspect = camera.aspectRatio;
	const float tanHalfFov = tan(glm::radians(camera.fov / 2.0f));

	//height + widht of near plane
	const float nearHeight = 2.0f * tanHalfFov * nearSplit;
	const float nearWidth = nearHeight * aspect;

	//height + width of far plane
	const float farHeight = 2.0f * tanHalfFov * farSplit;
	const float farWidth = farHeight * aspect;

	//define near face corners in view space
	corners[0] = glm::vec4(-nearWidth * 0.5f, -nearHeight * 0.5f, -nearSplit, 1.0f);
	corners[1] = glm::vec4(nearWidth * 0.5f, -nearHeight * 0.5f, -nearSplit, 1.0f);
	corners[2] = glm::vec4(nearWidth * 0.5f, nearHeight * 0.5f, -nearSplit, 1.0f);
	corners[3] = glm::vec4(-nearWidth * 0.5f, nearHeight * 0.5f, -nearSplit, 1.0f);

	//define far face corners in view space
	corners[4] = glm::vec4(-farWidth * 0.5f, -farHeight * 0.5f, -farSplit, 1.0f);
	corners[5] = glm::vec4(farWidth * 0.5f, -farHeight * 0.5f, -farSplit, 1.0f);
	corners[6] = glm::vec4(farWidth * 0.5f, farHeight * 0.5f, -farSplit, 1.0f);
	corners[7] = glm::vec4(-farWidth * 0.5f, farHeight * 0.5f, -farSplit, 1.0f);

	return corners;
}


//reinitialize depth buffer when cascade count changes
void reinitializeDepthBuffer()
{
	depthBuffer.Cleanup();
	depthBuffer.Initialize(screenWidth, screenHeight);
	calculateCascadeSplits();
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
	plane.load(ew::createPlane(60.0f, 60.0f, 100));
	light.position = glm::vec3(1.0f);
	light.color = glm::vec3(0.5f, 0.5f, 0.5f);
	light.rotating = false;
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
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Shadow Pass"))
	{
		ImGui::SliderFloat("Bias", &debug.bias, 0.0f, 0.1f);
		ImGui::SliderFloat("Min Bias", &debug.min_bias, 0.0f, 0.1f);
		ImGui::SliderFloat("Max Bias", &debug.max_bias, 0.0f, 0.1f);
		ImGui::Checkbox("Culling Front", &debug.cull_front);
		ImGui::Checkbox("Using PCF", &debug.use_pcf);

		ImGui::Separator(); //depth image
		ImGui::Text("Cascade Settings");

		ImGui::Checkbox("Show Cascade Colors", &debug.show_cascades);
		
		//slider for num of cascades
		int prev_num_cascades = debug.num_cascades;
		ImGui::SliderInt("Number of Cascades", &debug.num_cascades, 1, MAX_CASCADES);

		//reinitialize when cascade count changes
		if (prev_num_cascades != debug.num_cascades) 
		{
			reinitializeDepthBuffer();
			debug.cascade_to_view = std::min(debug.cascade_to_view, debug.num_cascades - 1);
		}

		//slider to pick map
		ImGui::Text("Cascade Shadow Maps:");
		int selected_cascade = debug.cascade_to_view;
		ImGui::SliderInt("Cascade to view", &selected_cascade, 0, MAX_CASCADES);
		debug.cascade_to_view = selected_cascade;

		//display chosen cascade shadow map
		ImGui::Image((ImTextureID)(intptr_t)depthBuffer.depthTextures[debug.cascade_to_view], ImVec2(256, 256));
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