#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;

ew::Camera camera;	//our camera
ew::CameraController cameraController;
ew::Transform monkeyTransform;

// Post-processing effect selection
int currentEffect = 0;

// Effect parameters
float blurRadius = 2.0f;
float chromaticStrength = 0.01f;
float gammaValue = 2.2f;
float grainIntensity = 0.1f;
float sharpenStrength = 0.5f;
float edgeThreshold = 0.1f;
float hdrExposure = 1.0f;
int toneMappingType = 0; // 0 = Reinhard, 1 = ACES
float vignetteIntensity = 0.5f;
float vignetteRadius = 0.5f;
float distortionStrength = 0.3f;
int distortionType = 0; // 0 = Barrel, 1 = Pincushion

// Fog effect parameters
float fogDensity = 0.1f;
float fogStart = 1.0f;
float fogEnd = 10.0f;
float fogColor[3] = { 0.8f, 0.8f, 0.9f }; // Light grayish-blue fog
float nearPlane = 0.1f;
float farPlane = 100.0f;

// Blur type selection
int blurType = 0; // 0 = Box Blur, 1 = Gaussian Blur

//for material controls:
struct Material
{
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shininess = 128;
}material;


struct FrameBuffer
{
	GLuint fbo;
	GLuint color0;
	GLuint color1;
	GLuint depth;
}framebuffer;

struct FullScreenQuad
{
	GLuint vao;
	GLuint vbo;
}fullscreen_quad;

void render(ew::Shader& shader, ew::Model& model)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
	glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	shader.use();
	shader.setInt("_MainTexture", 0);

	// Set material properties
	shader.setFloat("_Material.Ka", material.Ka);
	shader.setFloat("_Material.Kd", material.Kd);
	shader.setFloat("_Material.Ks", material.Ks);
	shader.setFloat("_Material.Shininess", material.Shininess);

	// Set camera matrices
	shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

	// Update model rotation
	monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));

	// Set the model transformation matrix
	shader.setMat4("_Model", monkeyTransform.modelMatrix());

	// Draw the model
	model.draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(0);
}


static float quad_vertices[] = {
	//	  X		 Y	   U	 V
		-1.0f,  1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,

		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
};

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);

	// Shader, model, and texture initialization
	ew::Shader litShader = ew::Shader("assets/lit.vert", "assets/lit.frag");	// Shader for 3D rendering

	// Post-processing shaders
	ew::Shader fullShader = ew::Shader("assets/full.vert", "assets/full.frag");	// No effect (passthrough)
	ew::Shader inverseShader = ew::Shader("assets/inverse.vert", "assets/inverse.frag");
	ew::Shader grayscaleShader = ew::Shader("assets/grayscale.vert", "assets/grayscale.frag");
	ew::Shader boxBlurShader = ew::Shader("assets/blur.vert", "assets/blur.frag");
	ew::Shader gaussianBlurShader = ew::Shader("assets/blur.vert", "assets/gaussianBlur.frag");
	ew::Shader chromaticShader = ew::Shader("assets/chromatic.vert", "assets/chromatic.frag");
	ew::Shader gammaShader = ew::Shader("assets/blur.vert", "assets/gamma.frag");
	ew::Shader filmGrainShader = ew::Shader("assets/blur.vert", "assets/filmGrain.frag");
	ew::Shader sharpenShader = ew::Shader("assets/blur.vert", "assets/sharpen.frag");
	ew::Shader edgeDetectShader = ew::Shader("assets/blur.vert", "assets/edgeDetect.frag");
	ew::Shader hdrShader = ew::Shader("assets/blur.vert", "assets/HDR.frag");
	ew::Shader vignetteShader = ew::Shader("assets/blur.vert", "assets/vignette.frag");
	ew::Shader lensDistortionShader = ew::Shader("assets/blur.vert", "assets/lensDistortion.frag");
	ew::Shader fogShader = ew::Shader("assets/blur.vert", "assets/fog.frag");

	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");					// Model

	GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");		// Texture

	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);	// Look at center of scene
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;								// Vertical field of view in degrees

	// Full-screen quad setup
	glGenVertexArrays(1, &fullscreen_quad.vao);
	glBindVertexArray(fullscreen_quad.vao);

	glGenBuffers(1, &fullscreen_quad.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, fullscreen_quad.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Initialize framebuffer
	glGenFramebuffers(1, &framebuffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

	glGenTextures(1, &framebuffer.color0);
	glBindTexture(GL_TEXTURE_2D, framebuffer.color0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 800, 600, 0, GL_RGB, GL_FLOAT, nullptr); // Changed to GL_RGB16F for HDR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color0, 0);

	// Change depth attachment to a texture for post-processing
	glGenTextures(1, &framebuffer.depth);
	glBindTexture(GL_TEXTURE_2D, framebuffer.depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 800, 600, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depth, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer incomplete:\n");
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);	// Unbind framebuffer

	// Remove the redundant draw call after the if-else block in the main loop

	while (!glfwWindowShouldClose(window)) {

		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		// Clear the screen
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cameraController.move(window, &camera, deltaTime);

		glActiveTexture(GL_TEXTURE0);  // Activate texture unit 0
		glBindTexture(GL_TEXTURE_2D, brickTexture);  // Bind the texture

		// Render the 3D scene to framebuffer (always do this for consistency)
		render(litShader, monkeyModel);

		// Apply post-processing effect
		if (currentEffect == 0) {
			// No post-processing, just render the lit scene directly to the screen
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glEnable(GL_DEPTH_TEST);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, brickTexture);

			litShader.use();
			litShader.setInt("_MainTexture", 0);
			litShader.setFloat("_Material.Ka", material.Ka);
			litShader.setFloat("_Material.Kd", material.Kd);
			litShader.setFloat("_Material.Ks", material.Ks);
			litShader.setFloat("_Material.Shininess", material.Shininess);
			litShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
			litShader.setMat4("_Model", monkeyTransform.modelMatrix());

			monkeyModel.draw();
		}
		else {
			// Apply the selected post-processing effect
			ew::Shader* currentShader = &fullShader; // Default to full shader if something goes wrong

			switch (currentEffect) {
			case 1: // Inverse
				currentShader = &inverseShader;
				break;
			case 2: // Grayscale
				currentShader = &grayscaleShader;
				break;
			case 3: // Blur
				// Select blur type based on user selection
				if (blurType == 0) {
					currentShader = &boxBlurShader;
				}
				else {
					currentShader = &gaussianBlurShader;
				}
				break;
			case 4: // Chromatic Aberration
				currentShader = &chromaticShader;
				break;
			case 5: // Gamma Correction
				currentShader = &gammaShader;
				break;
			case 6: // Film Grain
				currentShader = &filmGrainShader;
				break;
			case 7: // Sharpen
				currentShader = &sharpenShader;
				break;
			case 8: // Edge Detection
				currentShader = &edgeDetectShader;
				break;
			case 9: // HDR Tone Mapping
				currentShader = &hdrShader;
				break;
			case 10: // Vignette
				currentShader = &vignetteShader;
				break;
			case 11: // Lens Distortion
				currentShader = &lensDistortionShader;
				break;
			case 12: // Fog
				currentShader = &fogShader;
				break;
			}

			currentShader->use();
			currentShader->setInt("_MainTexture", 0);       // Texture unit 0
			currentShader->setFloat("_Time", time);         // Pass time for effects that need it (like film grain)

			// Set effect-specific parameters
			if (currentEffect == 3) { // Blur
				currentShader->setFloat("_BlurRadius", blurRadius);
			}
			else if (currentEffect == 4) { // Chromatic Aberration
				currentShader->setFloat("_ChromaticStrength", chromaticStrength);
			}
			else if (currentEffect == 5) { // Gamma Correction
				currentShader->setFloat("_Gamma", gammaValue);
			}
			else if (currentEffect == 6) { // Film Grain
				currentShader->setFloat("_GrainIntensity", grainIntensity);
			}
			else if (currentEffect == 7) { // Sharpen
				currentShader->setFloat("_SharpenStrength", sharpenStrength);
			}
			else if (currentEffect == 8) { // Edge Detection
				currentShader->setFloat("_EdgeThreshold", edgeThreshold);
			}
			else if (currentEffect == 9) { // HDR Tone Mapping
				currentShader->setFloat("_Exposure", hdrExposure);
				currentShader->setInt("_ToneMappingType", toneMappingType);
			}
			else if (currentEffect == 10) { // Vignette
				currentShader->setFloat("_VignetteIntensity", vignetteIntensity);
				currentShader->setFloat("_VignetteRadius", vignetteRadius);
			}
			else if (currentEffect == 11) { // Lens Distortion
				currentShader->setFloat("_DistortionStrength", distortionStrength);
				currentShader->setInt("_DistortionType", distortionType);
			}
			else if (currentEffect == 12) { // Fog
				currentShader->setFloat("_FogDensity", fogDensity);
				currentShader->setFloat("_FogStart", fogStart);
				currentShader->setFloat("_FogEnd", fogEnd);
				currentShader->setVec3("_FogColor", glm::vec3(fogColor[0], fogColor[1], fogColor[2]));
				currentShader->setFloat("_NearPlane", nearPlane);
				currentShader->setFloat("_FarPlane", farPlane);

				// Set depth texture
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, framebuffer.depth);
				currentShader->setInt("_DepthTexture", 1);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDisable(GL_DEPTH_TEST);

			glBindVertexArray(fullscreen_quad.vao);
			glActiveTexture(GL_TEXTURE0);  // Activate texture unit 0
			glBindTexture(GL_TEXTURE_2D, framebuffer.color0);  // Bind the framebuffer texture
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		// Draw the UI
		drawUI();

		// Swap buffers
		glfwSwapBuffers(window);
	}

	// Clean up resources
	glDeleteVertexArrays(1, &fullscreen_quad.vao);
	glDeleteBuffers(1, &fullscreen_quad.vbo);
	glDeleteFramebuffers(1, &framebuffer.fbo);
	glDeleteTextures(1, &framebuffer.color0);
	glDeleteTextures(1, &framebuffer.depth);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();

	printf("Shutting down...");
	return 0;
}


void resetCamera(ew::Camera* camera, ew::CameraController* controller)
{
	// Reset position
	camera->position = glm::vec3(0, 0, 5.0f);
	// Reset target
	camera->target = glm::vec3(0);

	// Reset controller rotation
	controller->yaw = controller->pitch = 0;
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

	if (ImGui::CollapsingHeader("Material"))
	{
		ImGui::SliderFloat("Ambient K", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("Diffuse K", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("Specular K", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}

	// Add post-processing effect selection
	if (ImGui::CollapsingHeader("Post-Processing"))
	{
		// Combo box for selecting the effect
		const char* items[] = {
			"None", "Inverse", "Grayscale", "Blur", "Chromatic Aberration",
			"Gamma Correction", "Film Grain", "Sharpen", "Edge Detection",
			"HDR Tone Mapping", "Vignette", "Lens Distortion", "Fog"
		};
		ImGui::Combo("Effect", &currentEffect, items, IM_ARRAYSIZE(items));

		// Effect-specific parameters
		ImGui::Separator();
		ImGui::Text("Effect Parameters");

		switch (currentEffect)
		{
		case 3: // Blur
		{
			const char* blurTypes[] = { "Box Blur", "Gaussian Blur" };
			ImGui::Combo("Blur Type", &blurType, blurTypes, IM_ARRAYSIZE(blurTypes));
			ImGui::SliderFloat("Blur Radius", &blurRadius, 0.0f, 10.0f);
		}
		break;
		case 4: // Chromatic Aberration
			ImGui::SliderFloat("Aberration Strength", &chromaticStrength, 0.0f, 0.1f);
			break;
		case 5: // Gamma Correction
			ImGui::SliderFloat("Gamma Value", &gammaValue, 0.1f, 5.0f);
			break;
		case 6: // Film Grain
			ImGui::SliderFloat("Grain Intensity", &grainIntensity, 0.0f, 1.0f);
			break;
		case 7: // Sharpen
			ImGui::SliderFloat("Sharpen Strength", &sharpenStrength, 0.0f, 2.0f);
			break;
		case 8: // Edge Detection
			ImGui::SliderFloat("Edge Threshold", &edgeThreshold, 0.01f, 0.5f);
			break;
		case 9: // HDR Tone Mapping
		{
			ImGui::SliderFloat("Exposure", &hdrExposure, 0.1f, 5.0f);
			const char* mappingTypes[] = { "Reinhard", "ACES" };
			ImGui::Combo("Tone Mapping Type", &toneMappingType, mappingTypes, IM_ARRAYSIZE(mappingTypes));
		}
		break;
		case 10: // Vignette
			ImGui::SliderFloat("Vignette Intensity", &vignetteIntensity, 0.0f, 1.0f);
			ImGui::SliderFloat("Vignette Radius", &vignetteRadius, 0.0f, 1.0f);
			break;
		case 11: // Lens Distortion
		{
			ImGui::SliderFloat("Distortion Strength", &distortionStrength, 0.0f, 1.0f);
			const char* distortionTypes[] = { "Barrel", "Pincushion" };
			ImGui::Combo("Distortion Type", &distortionType, distortionTypes, IM_ARRAYSIZE(distortionTypes));
		}
		break;
		case 12: // Fog
		{
			ImGui::SliderFloat("Fog Density", &fogDensity, 0.0f, 1.0f);
			ImGui::SliderFloat("Fog Start", &fogStart, 0.0f, 20.0f);
			ImGui::SliderFloat("Fog End", &fogEnd, 1.0f, 100.0f);
			ImGui::ColorEdit3("Fog Color", fogColor);
			ImGui::SliderFloat("Near Plane", &nearPlane, 0.01f, 1.0f);
			ImGui::SliderFloat("Far Plane", &farPlane, 10.0f, 1000.0f);
		}
		break;
		}
	}

	ImGui::Image((ImTextureID)(intptr_t)framebuffer.color0, ImVec2(800, 600), ImVec2(0, 1), ImVec2(1, 0));

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
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}