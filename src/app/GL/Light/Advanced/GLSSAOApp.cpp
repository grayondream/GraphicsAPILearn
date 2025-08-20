#include "GLSSAOApp.hpp"
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
#include <glm/gtc/random.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"


using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLSSAOApp::~GLSSAOApp() {
	m_cube->destroy();
    m_plane->destroy();
    // 释放帧缓冲资源
    if (m_gBuffer.gbuffer) glDeleteFramebuffers(1, &m_gBuffer.gbuffer);
    if (m_gBuffer.gPosition) glDeleteTextures(1, &m_gBuffer.gPosition);
    if (m_gBuffer.gNormal) glDeleteTextures(1, &m_gBuffer.gNormal);
    if (m_gBuffer.gAlbedoSpec) glDeleteTextures(1, &m_gBuffer.gAlbedoSpec);
    if (m_gBuffer.rboDepth) glDeleteRenderbuffers(1, &m_gBuffer.rboDepth);
    // 释放quad的VAO/VBO
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
}

void GLSSAOApp::initShapes() {
	m_cube = std::make_shared< GLCube>();
	m_plane = std::make_shared< GLPlane>();

	m_cube->init();
	m_plane->init();
}

void GLSSAOApp::createFrameBuffers() {
	unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gAlbedo;
    // position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    // normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    // color + specular color buffer
    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GetWindowWidth(), GetWindowHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);
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
		SPDLOG_ERROR("Framebuffer not complete!");
		ExitIfFailed(false, "Framebuffer not complete!");
	}
        
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_gBuffer.gbuffer = gBuffer;
	m_gBuffer.gPosition = gPosition;
	m_gBuffer.gNormal = gNormal;
	m_gBuffer.gAlbedoSpec = gAlbedo;
	m_gBuffer.rboDepth = rboDepth;
}

void GLSSAOApp::createQuadBuffer(){
	unsigned int quadVAO;
	unsigned int quadVBO;
	float quadVertices[] = {
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
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

bool GLSSAOApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	createTextures();
	compileShader();
	initShapes();
	createFrameBuffers();
	createQuadBuffer();
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

void GLSSAOApp::createTextures(){
	m_woodTexture = CreateTexture("wood.png");
	m_brickTexture = CreateTexture("bricks2.jpg");
}

void GLSSAOApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "SSAO");
	{
		const auto vfile = join(shaderDir, "GBuffer.vs");
		const auto ffile = join(shaderDir, "GBuffer.fs");
		auto ret = m_gBufferProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Light.vs");
		const auto ffile = join(shaderDir, "Light.fs");
		auto ret = m_lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "LightBox.vs");
		const auto ffile = join(shaderDir, "LightBox.fs");
		auto ret = m_lightBoxProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

static auto GetPositions(int count = 20, float gap = 0.04f, glm::vec3 center = glm::vec3(0)) {
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


static auto GetColorByPos(const glm::vec3& pos) {
    // 将位置坐标归一化到 [0, 1] 范围（可根据实际场景调整映射范围）
    // 示例：假设位置在 [-5, 5] 范围内，先映射到 [0, 1]
    auto normalize = [](float val, float min = -5.0f, float max = 5.0f) {
        return glm::clamp((val - min) / (max - min), 0.0f, 1.0f);
    };

    // x 分量映射到红色，y 映射到绿色，z 映射到蓝色
    float r = normalize(pos.x);       // 红色随 x 变化
    float g = normalize(pos.y);       // 绿色随 y 变化
    float b = normalize(pos.z);       // 蓝色随 z 变化

    // 可添加偏移或缩放增强效果（例如让绿色更明显）
    g = glm::pow(g, 0.8f);  // 绿色曲线调整，使中间值更亮

    return glm::vec3(r, g, b);
}

static std::vector<std::pair<glm::vec3, glm::vec3>> GetLightPosAndColors(int count = 20, float gap = 0.04f, glm::vec3 center = glm::vec3(0)){
	std::vector<std::pair<glm::vec3, glm::vec3>> lightPosAndColors;
	for (int x = 0; x < count; ++x) {
        for (int z = 0; z < count; ++z) {
            // 计算每个立方体的位置
            float posX = center.x + (x - (count - 1) / 2.0f) * gap;
            float posZ = center.z + (z - (count - 1) / 2.0f) * gap;
			const auto pos = glm::vec3(posX, center.y, posZ);
            lightPosAndColors.emplace_back(pos, GetColorByPos(pos));
        }
    }
	return lightPosAndColors;
}

void GLSSAOApp::renderQuad(){
	glBindVertexArray(m_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void GLSSAOApp::renderOneCube(GLProgram& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view){
	program.use();
	glBindVertexArray(m_cube->getVao());
	program.update("model", model);
	program.update("projection", projection);
	program.update("view", view);
	glDrawElements(GL_TRIANGLES, m_cube->idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLSSAOApp::renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	auto cubePositions = GetPositions(m_Count, 1, glm::vec3(0,0.5,0));
	program.use();
	program.update("viewPos", viewPos);
    m_woodTexture->texture()->bind(0);
    program.update("diffuseTexture", 0);
	for (const auto& pos : cubePositions) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first	
		model = glm::translate(model, pos);
		model = glm::scale(model, glm::vec3(0.1f));
		renderOneCube(program, model, projection, view);
	}
}

void GLSSAOApp::renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
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

void GLSSAOApp::renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view) {
    auto lightPositionsColor = GetLightPosAndColors(m_Count, 2);
	program.use();
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer.gPosition);
		program.update("gPosition", 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer.gNormal);
		program.update("gNormal", 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer.gAlbedoSpec);
		program.update("gAlbedoSpec", 2);
	}

	program.update("viewPos", _camera.getAttr().pos);
	const float linear = 0.7f;
	const float quadratic = 1.8f;
	const float constant = 1.0f;
	program.update("enableVolume", m_enableVolume);
	for (unsigned int i = 0; i < lightPositionsColor.size(); i++){
		program.update("lights[" + std::to_string(i) + "].Position", lightPositionsColor[i].first);
		program.update("lights[" + std::to_string(i) + "].Color", lightPositionsColor[i].second);
		// update attenuation parameters and calculate radius
		program.update("lights[" + std::to_string(i) + "].Linear", linear);
		program.update("lights[" + std::to_string(i) + "].Quadratic", quadratic);
		const float maxBrightness = std::fmaxf(std::fmaxf(lightPositionsColor[i].second.r, lightPositionsColor[i].second.g), lightPositionsColor[i].second.b);
        float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
        program.update("lights[" + std::to_string(i) + "].Radius", radius);
	}

	renderQuad();
}

void GLSSAOApp::renderLightBox(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view) {
	program.use();
	auto lightPositionsColor = GetLightPosAndColors(m_Count, 1);
	for (const auto& posColor : lightPositionsColor) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, posColor.first);
		model = glm::scale(model, glm::vec3(0.1f));
		program.update("lightColor", posColor.second);
		renderOneCube(program, model, projection, view);
	}
}

void GLSSAOApp::renderGBuffer(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view) {
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


void GLSSAOApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
	renderLight(m_lightProgram, projection, view);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gBuffer.gbuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
	glBlitFramebuffer(0, 0, GetWindowWidth(), GetWindowHeight(), 0, 0, GetWindowWidth(), GetWindowHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	renderLightBox(m_lightBoxProgram, projection, view);

	ImGui::Begin("OpenGL");
	ImGui::SliderInt("Cube Count", &m_Count, 1, 13);
	ImGui::Checkbox("Enable Volume", &m_enableVolume);
	ImGui::End();
}