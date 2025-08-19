#include "GLDeferApp.hpp"
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

GLDeferApp::~GLDeferApp() {
	m_cube->destroy();
	m_plane->destroy();
}

void GLDeferApp::initShapes() {
	m_cube = std::make_shared< GLCube>();
	m_plane = std::make_shared< GLPlane>();

	m_cube->init();
	m_plane->init();
}

void GLDeferApp::createFrameBuffers() {
	unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gAlbedoSpec;
    // position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    // normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    // color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, GetWindowWidth(), GetWindowHeight());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		SPDLOG_INFO("Framebuffer not complete!");
	}
        
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_gBuffer.gbuffer = gBuffer;
	m_gBuffer.gPosition = gPosition;
	m_gBuffer.gNormal = gNormal;
	m_gBuffer.gAlbedoSpec = gAlbedoSpec;
	m_gBuffer.rboDepth = rboDepth;
}

bool GLDeferApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	createTextures();
	compileShader();
	initShapes();
	createFrameBuffers();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
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
void GLDeferApp::createTextures(){
	m_woodTexture = CreateTexture("wood.png");
	m_brickTexture = CreateTexture("bricks2.jpg");
}

void GLDeferApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Defer");
	{
		const auto vfile = join(shaderDir, "GBuffer.vs");
		const auto ffile = join(shaderDir, "GBuffer.fs");
		auto ret = m_gBufferProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	// {
	// 	const auto vfile = join(shaderDir, "Light.vs");
	// 	const auto ffile = join(shaderDir, "Light.fs");
	// 	auto ret = m_lightProgram.init(vfile, ffile);
	// 	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	// }

	// {
	// 	const auto vfile = join(shaderDir, "Blur.vs");
	// 	const auto ffile = join(shaderDir, "Blur.fs");
	// 	auto ret = m_blurProgram.init(vfile, ffile);
	// 	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	// }

	// {
	// 	const auto vfile = join(shaderDir, "Final.vs");
	// 	const auto ffile = join(shaderDir, "Final.fs");
	// 	auto ret = m_finalProgram.init(vfile, ffile);
	// 	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	// }
}

static auto GetCubePositions(int count = 20, float gap = 0.04f, glm::vec3 center = glm::vec3(0)) {
    std::vector<glm::vec3> cubePositions;

    for (int x = 0; x < count; ++x) {
        for (int z = 0; z < count; ++z) {
            // 计算每个立方体的位置
            float posX = center.x + (x - (count - 1) / 2.0f) * gap;
            float posZ = center.z + (z - (count - 1) / 2.0f) * gap;
            cubePositions.emplace_back(posX, center.y, posZ);
        }
    }

    return cubePositions;
}

void GLDeferApp::renderOneCube(GLProgram& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view){
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

void GLDeferApp::renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	auto cubePositions = GetCubePositions(m_cubeCount);
	program.use();
	program.update("viewPos", viewPos);
	for (const auto& pos : cubePositions) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first	
		model = glm::translate(model, pos);
		model = glm::scale(model, glm::vec3(0.02f));
		renderOneCube(program, model, projection, view);
	}
}

void GLDeferApp::renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
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

void GLDeferApp::renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view) {
	auto lightPosAndColor = GetLightPosAndColor();
	const auto& lightPositions = lightPosAndColor.first;
	const auto& lightColors = lightPosAndColor.second;
	program.use();
	for (int i = 0; i < lightPositions.size(); i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first	
		model = glm::translate(model, lightPositions[i]);
		model = glm::scale(model, glm::vec3(0.010f));
		program.update("lightColor", lightColors[i]);
		renderOneCube(program, model, projection, view);
	}
}

void GLDeferApp::renderGBuffer(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view) {
	glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer.gbuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	program.use();
	program.update("projection", projection);
	program.update("view", view);
	m_woodTexture->texture()->bind(0);
	program.update("diffuseTexture", 0);
	renderCubes(program, projection, view, _camera.getAttr().pos);
	m_brickTexture->texture()->bind(0);
	program.update("diffuseTexture", 0);
	renderPlane(program, projection, view, _camera.getAttr().pos);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void GLDeferApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	ImGui::Begin("OpenGL");
	ImGui::SliderInt("Cube Count", &m_cubeCount, 10, 200);
	ImGui::End();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
}
