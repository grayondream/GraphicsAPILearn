#include "GLIBLIrradianceApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "native/GL/GLImageTexture2D.hpp"
#include "native/GL/GLCube.hpp"
#include "native/GL/GLPlane.hpp"
#include "base/Log.hpp"
#include "imgui.h"
#include "utils/FileUtils.hpp"
#include "geometry/Rect.hpp"
#include "utils/GL/GLUtils.hpp"
#include "base/Constexpr.hpp"
#include <stb_image.h>

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLIBLIrradianceApp::~GLIBLIrradianceApp() {
    
}

void GLIBLIrradianceApp::initShapes() {
	m_sphere.init();	
	m_cube.init();
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgFile, bool isHdr = false){
	TextureOption option{};
	option.IsHdr = isHdr;
	auto texture = std::make_shared<GLImageTexture2D>(imgFile, option);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
}

bool GLIBLIrradianceApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	compileShader();
	initShapes();	
	loadTexture();
	initFramebuffer();
	initCaptureViews();
	glEnable(GL_DEPTH_TEST);\
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLIBLIrradianceApp::initCaptureViews(){
	unsigned int envCubemap;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_envCubemap = envCubemap;
}

void GLIBLIrradianceApp::createIrradianceMap(){
    // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    unsigned int irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    m_irradianceMap = irradianceMap;
}

void GLIBLIrradianceApp::initFramebuffer(){
	unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_captureFBO = captureFBO;
	m_captureRBO = captureRBO;
}

void GLIBLIrradianceApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "IBL_Irradiance");
	{
		const auto vfile = join(shaderDir, "CUBE.vs");
		const auto ffile = join(shaderDir, "CUBE.fs");
		auto ret = m_cubeMapProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}


	{
		const auto vfile = join(shaderDir, "PBR.vs");
		const auto ffile = join(shaderDir, "PBR.fs");
		auto ret = m_program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Background.vs");
		const auto ffile = join(shaderDir, "Background.fs");
		auto ret = m_backgroundProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Irradiance.vs");
		const auto ffile = join(shaderDir, "Irradiance.fs");
		auto ret = m_irradianceProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");

	}
}

static std::vector<glm::mat4> GetCaptureViews(){
	glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    return std::vector<glm::mat4>(captureViews, captureViews + 6);
}

static auto GetLightPosAndColor(){
	std::vector<glm::vec3> lightPositions = {
        glm::vec3(-10.0f,  10.0f, 10.0f),
        glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    std::vector<glm::vec3> lightColors = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f)
    };

    return std::make_pair(lightPositions, lightColors);
}


void GLIBLIrradianceApp::renderToCubemap(){
	glViewport(0, 0, 512, 512);
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	const auto captureViews = GetCaptureViews();
	m_cubeMapProgram.use();
	m_hdrEnvTexture->texture()->bind(0);
    m_cubeMapProgram.update("equirectangularMap", 0);
    m_cubeMapProgram.update("projection", captureProjection);
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    for (unsigned int i = 0; i < 6; ++i){
        m_cubeMapProgram.update("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube(m_cubeMapProgram, glm::mat4(1.0));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//GLUtils::SaveFramebufferAsImage(m_captureFBO, 512, 512);
	glViewport(0, 0, m_window->getProperties().width, m_window->getProperties().height);
}

void GLIBLIrradianceApp::renderIrradianceMap(){
	m_irradianceProgram.use();
    m_irradianceProgram.update("environmentMap", 0);
	//should be same as capture projection in renderToCubemap
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    m_irradianceProgram.update("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);

    glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
	const auto captureViews = GetCaptureViews();
    for (unsigned int i = 0; i < 6; ++i)
    {
        m_irradianceProgram.update("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube(m_irradianceProgram, glm::mat4(1.0));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_window->getProperties().width, m_window->getProperties().height);
}

void GLIBLIrradianceApp::renderCube(GLProgram &program, const glm::mat4 &model) {
	program.use();
	program.update("model", model);
	glBindVertexArray(m_cube.getVao());
	glDrawElements(GL_TRIANGLES, m_cube.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLIBLIrradianceApp::renderSphere(GLProgram& program, const glm::mat4& model) {
	program.use();
	program.update("model", model);
	const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
	program.update("normalMatrix", normal);
	glBindVertexArray(m_sphere.getVao());
	glDrawElements(GL_TRIANGLES, m_sphere.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

static std::vector<glm::vec3> GenreateObjPos(int radius = 5, float gap = 0.5f, const glm::vec3 &center = glm::vec3(0.0f)) {
    std::vector<glm::vec3> positions;
    if (radius < 0) {
        return positions;
    }

    for (int row = -radius; row <= radius; ++row) {
        for (int col = -radius; col <= radius; ++col) {
            float x = center.x + static_cast<float>(col) * gap;
            float y = center.y + static_cast<float>(row) * gap;
            positions.push_back(glm::vec3(x, y, center.z));
        }
    }
    
    return positions;
}

void GLIBLIrradianceApp::loadTexture(){
	auto resDir = StaticCollector::getImagePath();
    m_hdrEnvTexture = CreateTexture(join(resDir, "newport_loft.hdr"), true);
}

void GLIBLIrradianceApp::renderBeforeLoop(){
	renderToCubemap();
	createIrradianceMap();
	renderIrradianceMap();
}

void GLIBLIrradianceApp::renderBackground(GLProgram& program, const glm::mat4& view, const glm::mat4& projection) {
	program.use();
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
	program.update("view", view);
	program.update("environmentMap", 0);
	program.update("projection", projection);
	renderCube(program, glm::mat4(1.0));
}

void GLIBLIrradianceApp::renderObjectsAndLights(GLProgram &program, const glm::mat4 &view, const glm::mat4 &projection) {
	const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));
	auto pos = _camera.getAttr().pos;
	program.use();
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);

	program.update("texture", 0);
    program.update("irradianceMap", 0);
	program.update("projection", projection);
	program.update("view", view);
	program.update("camPos", pos);
	program.update("roughness", m_roughness);
    program.update("metallic", m_metallic);
	program.update("ao", m_ao);
	const int cnt = objPos.size();
	for(int i = 0;i < cnt;i ++){
		const auto pos = objPos[i];
		program.update("albedo", glm::vec3(i * 1.0f/ cnt, 0.0f, 0.0f));
		auto objectPos = glm::mat4(1.0f);
		objectPos = glm::translate(objectPos, pos);
		objectPos = glm::scale(objectPos, glm::vec3(0.4f));
		renderSphere(program, objectPos);
	}

	const auto lightPosAndColor = GetLightPosAndColor();
	for (int i = 0; i < lightPosAndColor.first.size(); ++i) {
		const auto lightPos = lightPosAndColor.first[i];
		auto lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, lightPos);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f));
		program.update("lightPositions[" + std::to_string(i) + "]", lightPosAndColor.first[i]);
		program.update("lightColors[" + std::to_string(i) + "]", lightPosAndColor.second[i]);
		renderSphere(program, lightModel);
	}
}

void GLIBLIrradianceApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	renderObjectsAndLights(m_program, view, projection);
	renderBackground(m_backgroundProgram, view, projection);

	ImGui::Begin("OpenGL");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
	ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
	ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
	ImGui::End();
}