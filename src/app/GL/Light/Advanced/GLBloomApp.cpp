#include "GLBloomApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Native/GL/GLCube.hpp"
#include "Native/GL/GLPlane.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLBloomApp::~GLBloomApp() {
	m_cube->destroy();
	m_plane->destroy();
}

void GLBloomApp::initShapes() {
	m_cube = std::make_shared< GLCube>();
	m_plane = std::make_shared< GLPlane>();

	m_cube->init();
	m_plane->init();
}

static auto CreateHdrFrameBuffer(int width, int height) {
	unsigned int hdrFBO;
	glGenFramebuffers(1, &hdrFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	// create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
	unsigned int colorBuffers[2];
	glGenTextures(2, colorBuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// attach texture to framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
	}
	// create and attach depth buffer (renderbuffer)
	unsigned int rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);
	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		ErrorHandle::ExitIfFailed(false, "Failed to crate frame buffer");
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return std::pair(hdrFBO, std::pair(colorBuffers[0], colorBuffers[1]));
}

static auto CreateBloomFrameBuffer(int width, int height) {
	unsigned int pingpongFBO[2];
	unsigned int pingpongColorbuffers[2];
	glGenFramebuffers(2, pingpongFBO);
	glGenTextures(2, pingpongColorbuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
		glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
		// also check if framebuffers are complete (no need for depth buffer)
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			ErrorHandle::ExitIfFailed(false, "Failed to crate frame buffer");
		}
			

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	return std::pair(std::pair(pingpongFBO[0], pingpongFBO[1]), std::pair(pingpongColorbuffers[0], pingpongColorbuffers[1]));
}

bool GLBloomApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	createTextures();
	compileShader();
	initShapes();
	const auto [fbo, buffers] = CreateHdrFrameBuffer(GetWindowWidth(), GetWindowHeight());
	m_hdrFBO = fbo;
	m_colorBuffers[0] = buffers.first;
	m_colorBuffers[1] = buffers.second;
	const auto [pfbos, ppcolorBuffers] = CreateBloomFrameBuffer(GetWindowWidth(), GetWindowHeight());
	m_pingpongFBO[0] = pfbos.first;
	m_pingpongFBO[1] = pfbos.second;
	m_pingpongColorbuffers[0] = ppcolorBuffers.first;
	m_pingpongColorbuffers[1] = ppcolorBuffers.second;
	createQuadBuffer();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLBloomApp::createQuadBuffer() {
	float quadVertices[] = {
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	unsigned int quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glBindVertexArray(0);
	m_quadVAO = quadVAO;
	m_quadVBO = quadVBO;
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgname){
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
}

static auto GetLightPosAndColor(){
	std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3( 0.0f, 0.5f,  1.5f));
    lightPositions.push_back(glm::vec3(-4.0f, 0.5f, -3.0f));
    lightPositions.push_back(glm::vec3( 3.0f, 0.5f,  1.0f));
    lightPositions.push_back(glm::vec3(-.8f,  2.4f, -1.0f));
    // colors
    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3(5.0f,   5.0f,  5.0f));
    lightColors.push_back(glm::vec3(10.0f,  0.0f,  0.0f));
    lightColors.push_back(glm::vec3(0.0f,   0.0f,  15.0f));
    lightColors.push_back(glm::vec3(0.0f,   5.0f,  0.0f));

	return std::pair(lightPositions, lightColors);
}
void GLBloomApp::createTextures(){
	m_woodTexture = CreateTexture("wood.png");
	m_brickTexture = CreateTexture("bricks2.jpg");
}

void GLBloomApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Bloom");
	{
		const auto vfile = join(shaderDir, "Bloom.vs");
		const auto ffile = join(shaderDir, "Bloom.fs");
		auto ret = m_bloomProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Light.vs");
		const auto ffile = join(shaderDir, "Light.fs");
		auto ret = m_lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Blur.vs");
		const auto ffile = join(shaderDir, "Blur.fs");
		auto ret = m_blurProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Final.vs");
		const auto ffile = join(shaderDir, "Final.fs");
		auto ret = m_finalProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

static auto GetCubePositions() {
	std::vector<glm::vec3> cubePositions = {
	  glm::vec3(0.0f,  0.0f,  0.0f),
	  glm::vec3(2.0f,  5.0f, -15.0f),
	  glm::vec3(-1.5f, -2.2f, -2.5f),
	  glm::vec3(-3.8f, -2.0f, -12.3f),
	  glm::vec3(2.4f, -0.4f, -3.5f),
	  glm::vec3(-1.7f,  3.0f, -7.5f),
	  glm::vec3(1.3f, -2.0f, -2.5f),
	  glm::vec3(1.5f,  2.0f, -2.5f),
	  glm::vec3(1.5f,  0.2f, -1.5f),
	  glm::vec3(-1.3f,  1.0f, -1.5f)
	};

	return cubePositions;
}

void GLBloomApp::renderOneCube(GLProgram& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view){
	program.use();
	glBindVertexArray(m_cube->getVao());
	program.update("model", model);
	program.update("projection", projection);
	program.update("view", view);
	m_woodTexture->texture()->bind(0);
	program.update("diffuseTexture", 0);
	glDrawElements(GL_TRIANGLES, m_cube->idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLBloomApp::renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	auto cubePositions = GetCubePositions();
	program.use();
	program.update("viewPos", viewPos);
	for (const auto& pos : cubePositions) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first	
		model = glm::translate(model, pos);
		model = glm::scale(model, glm::vec3(0.5f));
		renderOneCube(program, model, projection, view);
	}
}

void GLBloomApp::renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	glBindVertexArray(m_plane->getVao());
	{
		program.use();
		program.update("viewPos", viewPos);
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		m_brickTexture->texture()->bind(0);
		program.update("diffuseTexture", 0);
		model = glm::translate(model, glm::vec3(0.0));
		program.update("model", model);
		program.update("view", view);
		program.update("projection", projection);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}

#include <iostream>
#include <fstream>

void saveFramebufferAsImage(GLuint framebuffer, int width, int height) {
    // �� framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // ����һ�����������洢������������
    float* pixels = new float[width * height * 4]; // RGBA��ʽ

    // ��ȡ��������
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels);

    // ����Ϊͼ���ļ������� PPM ��ʽ����������������
    std::ofstream file("output.ppm", std::ios::binary);
    if (file) {
        file << "P6\n" << width << " " << height << "\n255\n";

        // ����������ת��Ϊ8λ���ݲ�д���ļ�
        for (int i = 0; i < width * height; ++i) {
            unsigned char r = static_cast<unsigned char>(std::clamp(pixels[i * 4 + 0] * 255.0f, 0.0f, 255.0f));
            unsigned char g = static_cast<unsigned char>(std::clamp(pixels[i * 4 + 1] * 255.0f, 0.0f, 255.0f));
            unsigned char b = static_cast<unsigned char>(std::clamp(pixels[i * 4 + 2] * 255.0f, 0.0f, 255.0f));
            file.write(reinterpret_cast<char*>(&r), 1);
            file.write(reinterpret_cast<char*>(&g), 1);
            file.write(reinterpret_cast<char*>(&b), 1);
        }

        file.close();
        std::cout << "Framebuffer saved as output.ppm" << std::endl;
    } else {
        std::cerr << "Failed to save image" << std::endl;
    }

    // ����
    delete[] pixels;

    // ��� framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ���ʵ��ĵط����ô˺���
void GLBloomApp::extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view) {
	glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderLight(m_lightProgram, projection, view);
	const auto viewPos = _camera.getAttr().pos;
	const auto [lightPositions, lightColors] = GetLightPosAndColor();
	m_bloomProgram.use();
	for (int i = 0; i < lightPositions.size(); i++) {
		m_bloomProgram.update("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
		m_bloomProgram.update("lights[" + std::to_string(i) + "].Color", lightColors[i]);
	}
	renderCubes(m_bloomProgram, projection, view, viewPos);
	renderPlane(m_bloomProgram, projection, view, viewPos);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//saveFramebufferAsImage(m_hdrFBO, GetWindowWidth(), GetWindowHeight());
}

void GLBloomApp::blurBrightPart() {
	m_blurProgram.use();
	m_blurProgram.update("image", 0);
	bool horizontal = true, first_iteration = true;
	for (unsigned int i = 0; i < 10; i++){
		glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[horizontal]);
		m_blurProgram.update("horizontal", horizontal);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? m_colorBuffers[1] : m_pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
		renderQuad();
		horizontal = !horizontal;
		if (first_iteration){
			first_iteration = false;
		}
		
		//saveFramebufferAsImage(m_pingpongFBO[horizontal], GetWindowWidth(), GetWindowHeight());
	}
    
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLBloomApp::renderFinal() {
	bool bloom = true;
	float exposure = 1.0f;
	m_finalProgram.use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	m_finalProgram.use();
	m_finalProgram.update("scene", 0);
	m_finalProgram.update("bloomBlur", 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colorBuffers[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_pingpongColorbuffers[1]);
	m_finalProgram.update("bloom", m_enableBloom);
	m_finalProgram.update("exposure", m_expose);
	renderQuad();
}

void GLBloomApp::renderQuad() {
	glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void GLBloomApp::renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view) {
	auto lightPosAndColor = GetLightPosAndColor();
	const auto& lightPositions = lightPosAndColor.first;
	const auto& lightColors = lightPosAndColor.second;
	program.use();
	for (int i = 0; i < lightPositions.size(); i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first	
		model = glm::translate(model, lightPositions[i]);
		model = glm::scale(model, glm::vec3(0.25f));
		program.update("lightColor", lightColors[i]);
		renderOneCube(program, model, projection, view);
		
	}
}

void GLBloomApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Bloom", &m_enableBloom);
	ImGui::SliderFloat("Expose Value", &m_expose, 0, 1.0);
	ImGui::End();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	extractBrightPart(projection, view);
	blurBrightPart();
	renderFinal();
}
