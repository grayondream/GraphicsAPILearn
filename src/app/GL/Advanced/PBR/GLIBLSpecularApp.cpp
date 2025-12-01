#include "GLIBLSpecularApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Native/GL/GLCube.hpp"
#include "Native/GL/GLPlane.hpp"
#include "Base/Log.hpp"
#include "imgui.h"
#include "Utils/FileUtils.hpp"
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"
#include <stb_image.h>

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLIBLSpecularApp::~GLIBLSpecularApp() {
    
}

void GLIBLSpecularApp::initShapes() {
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

bool GLIBLSpecularApp::initApp() {
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
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	return true;
}

void GLIBLSpecularApp::initCaptureViews(){
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

void GLIBLSpecularApp::createIrradianceMap(){
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

void GLIBLSpecularApp::initFramebuffer(){
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

void GLIBLSpecularApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "IBL_Specular");
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

	{
		const auto vfile = join(shaderDir, "Prefilter.vs");
		const auto ffile = join(shaderDir, "Prefilter.fs");
		auto ret = m_prefilterProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Brdf.vs");
		const auto ffile = join(shaderDir, "Brdf.fs");
		auto ret = m_brdfLUTProgram.init(vfile, ffile);
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


void GLIBLSpecularApp::renderToCubemap(){
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
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

glm::mat4 GetCaptureProjection(){
	return glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
}

void GLIBLSpecularApp::renderIrradianceMap(){
	m_irradianceProgram.use();
    m_irradianceProgram.update("environmentMap", 0);
	//should be same as capture projection in renderToCubemap
	glm::mat4 captureProjection = GetCaptureProjection();
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

void GLIBLSpecularApp::renderCube(GLProgram &program, const glm::mat4 &model) {
	program.use();
	program.update("model", model);
	glBindVertexArray(m_cube.getVao());
	glDrawElements(GL_TRIANGLES, m_cube.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLIBLSpecularApp::renderSphere(GLProgram& program, const glm::mat4& model) {
	program.use();
	program.update("model", model);
	const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
	program.update("normalMatrix", normal);
	glBindVertexArray(m_sphere.getVao());
	glDrawElements(GL_TRIANGLES, m_sphere.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLIBLSpecularApp::createQuadBuffer() {
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

void GLIBLSpecularApp::createPrefilterMap(){
	unsigned int prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i){
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	m_prefilterMap = prefilterMap;	
}

void GLIBLSpecularApp::renderPerfilterMap(){
	m_prefilterProgram.use();
    m_prefilterProgram.update("environmentMap", 0);
	glm::mat4 captureProjection = GetCaptureProjection();
    m_prefilterProgram.update("projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip){
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth  = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

		const auto captureViews = GetCaptureViews();
        float roughness = (float)mip / (float)(maxMipLevels - 1);
        m_prefilterProgram.update("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i){
            m_prefilterProgram.update("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube(m_prefilterProgram, glm::mat4(1.0));
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLIBLSpecularApp::renderBrdfLUT(){
	// pbr: generate a 2D LUT from the BRDF equations used.
    // ----------------------------------------------------
    unsigned int brdfLUTTexture;
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);
    m_brdfLUTProgram.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	m_brdfLUTTexture = brdfLUTTexture;
}

void GLIBLSpecularApp::loadTexture(){
	auto resDir = StaticCollector::getImagePath();
    m_hdrEnvTexture = CreateTexture(join(resDir, "newport_loft.hdr"), true);
}

void GLIBLSpecularApp::renderBeforeLoop(){
	renderToCubemap();
	createIrradianceMap();
	renderIrradianceMap();
	createPrefilterMap();
	renderPerfilterMap();
	renderBrdfLUT();
}

void GLIBLSpecularApp::renderQuad() {
	glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void GLIBLSpecularApp::renderBackground(GLProgram& program, const glm::mat4& view, const glm::mat4& projection) {
	program.use();
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
	program.update("view", view);
	program.update("environmentMap", 0);
	program.update("projection", projection);
	renderCube(program, glm::mat4(1.0));
}

void GLIBLSpecularApp::renderObjectsAndLights(GLProgram &program, const glm::mat4 &view, const glm::mat4 &projection) {
	const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));
	auto pos = _camera.getAttr().pos;
	program.use();
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);

	glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_brdfLUTTexture);

	program.update("prefilterMap", 1);
    program.update("brdfLUT", 2);
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

void GLIBLSpecularApp::drawScene(const float dt) {
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