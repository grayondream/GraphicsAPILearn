#include "GLSSAOApp.hpp"
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
#include "rhi/core/IShader.hpp"


using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLSSAOApp::~GLSSAOApp() {
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
	
}

static std::vector<glm::vec3> GenerateSSAONoise(int kernelSize = 64) {
	std::vector<glm::vec3> ssaoNoise;
	std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    for (unsigned int i = 0; i < kernelSize; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    return ssaoNoise;
}

void GLSSAOApp::createSSAOFbo() {
	unsigned int ssaoFBO, ssaoBlurFBO;
    glGenFramebuffers(1, &ssaoFBO);  glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    unsigned int ssaoColorBuffer, ssaoColorBufferBlur;
    // SSAO color buffer
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, GetWindowWidth(), GetWindowHeight(), 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		SPDLOG_ERROR("SSAO Framebuffer not complete!");
		ExitIfFailed(false, "SSAO Framebuffer not complete!");
	}

    // and blur stage
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, GetWindowWidth(), GetWindowHeight(), 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		SPDLOG_ERROR("SSAO Blur Framebuffer not complete!");
		ExitIfFailed(false, "SSAO Blur Framebuffer not complete!");
	}

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	{
		const auto ssaoNoise = GenerateSSAONoise();
		unsigned int noiseTexture; 
		glGenTextures(1, &noiseTexture);
		glBindTexture(GL_TEXTURE_2D, noiseTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		m_ssaoBuffer.noiseTexture = noiseTexture;
	}

	m_ssaoBuffer.fbo = ssaoFBO;
	m_ssaoBuffer.blurFbo = ssaoBlurFBO;
	m_ssaoBuffer.ssaoColorBuffer = ssaoColorBuffer;
	m_ssaoBuffer.ssaoBlurBuffer = ssaoColorBufferBlur;
}

void GLSSAOApp::createFrameBuffers() {
	createGBufferFbo();
	createSSAOFbo();	
}

void GLSSAOApp::createGBufferFbo() {
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

void GLSSAOApp::createCubeBuffer(){
	unsigned int cubeVAO;
	unsigned int cubeVBO;
	float vertices[] = {
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		// bottom face
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		// top face
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	// fill buffer
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	// link vertex attributes
	glBindVertexArray(cubeVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	m_cubeVAO = cubeVAO;
	m_cubeVBO = cubeVBO;
}
bool GLSSAOApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	loadModel();
	createTextures();
	compileShader();
	initModelPipeline();
	initShapes();
	createCubeBuffer();
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
		const auto vfile = join(shaderDir, "SSAO.vs");
		const auto ffile = join(shaderDir, "SSAO.fs");
		auto ret = m_ssaoProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "SSAOBlur.vs");
		const auto ffile = join(shaderDir, "SSAOBlur.fs");
		auto ret = m_ssaoBlurProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Light.vs");
		const auto ffile = join(shaderDir, "Light.fs");
		auto ret = m_lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

void GLSSAOApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	const auto modelFile = join(modelPath, "backpack", "backpack.obj");
	m_model = std::make_shared<Model>(renderer().get(), modelFile);
}

void GLSSAOApp::initModelPipeline() {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "SSAO");
	const auto vfile = join(shaderDir, "GBuffer.vs");
	const auto ffile = join(shaderDir, "GBuffer.fs");
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create GBuffer RHI shader failed: {}", shader->getLog());
	m_modelPipeline = renderer()->createPipeline(m_model->vertexLayout(), shader);
	m_modelPipeline->setDepthTest(true);
}

static float ourLerp(float a, float b, float f){
	return a + f * (b - a);
}

static std::vector<glm::vec3> GenerateSSAOKernel(int kernelSize = 64) {
	std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < kernelSize; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / float(kernelSize);

        // scale samples s.t. they're more aligned to center of kernel
        scale = ourLerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }
    return ssaoKernel;
}

void GLSSAOApp::renderQuad(){
	glBindVertexArray(m_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void GLSSAOApp::renderOneCube() {
	glBindVertexArray(m_cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void GLSSAOApp::renderGBuffer(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view) {
	glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer.gbuffer);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glm::mat4 model = glm::mat4(1.0f);
		m_gBufferProgram.use();
		m_gBufferProgram.update("projection", projection);
		m_gBufferProgram.update("view", view);
		// room cube
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0, 7.0f, 0.0f));
		model = glm::scale(model, glm::vec3(7.5f, 7.5f, 7.5f));
		m_gBufferProgram.update("model", model);
		m_gBufferProgram.update("invertedNormals", 1); // invert normals as we're inside the cube
		renderOneCube();
		m_gBufferProgram.update("invertedNormals", 0); 
		// backpack model on the floor
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0));
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
		model = glm::scale(model, glm::vec3(1.0f));
		renderer()->setPipeline(m_modelPipeline);
		m_modelPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		m_modelPipeline->setUniform("view", glm::value_ptr(view), 1);
		m_modelPipeline->setUniform("model", glm::value_ptr(model), 1);
		m_modelPipeline->setUniform("invertedNormals", 0);
		m_model->draw(renderer().get(), m_modelPipeline.get());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLSSAOApp::renderSSAOTexture(GLProgram &program, const glm::mat4 &projection){
	m_ssaoProgram.use();
    m_ssaoProgram.update("gPosition", 0);
    m_ssaoProgram.update("gNormal", 1);
    m_ssaoProgram.update("texNoise", 2);
	const auto ssaoKernel = GenerateSSAOKernel();
	const auto ssaoFBO = m_ssaoBuffer.fbo;
	const auto gPosition = m_gBuffer.gPosition;
	const auto gNormal = m_gBuffer.gNormal;
	const auto noiseTexture = m_ssaoBuffer.noiseTexture;
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		// Send kernel + rotation 
		for (unsigned int i = 0; i < 64; ++i){
			m_ssaoProgram.update("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
		}
			
		m_ssaoProgram.update("projection", projection);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, noiseTexture);
		renderQuad();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLSSAOApp::renderBlurSSAOTexture(GLProgram &program){
	m_ssaoBlurProgram.use();
    m_ssaoBlurProgram.update("ssaoInput", 0);
	const auto ssaoBlurFBO = m_ssaoBuffer.blurFbo;
	const auto ssaoColorBuffer = m_ssaoBuffer.ssaoColorBuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		m_ssaoBlurProgram.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
		renderQuad();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLSSAOApp::renderLightPass(GLProgram &program){
	m_lightProgram.use();
    m_lightProgram.update("gPosition", 0);
    m_lightProgram.update("gNormal", 1);
    m_lightProgram.update("gAlbedo", 2);
    m_lightProgram.update("ssao", 3);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// send light relevant uniforms
	const glm::vec3 lightPos = glm::vec3(2.0, 4.0, -2.0);
	const glm::vec3 lightColor = glm::vec3(0.2, 0.2, 0.7);
	glm::vec3 lightPosView = glm::vec3(_camera.getViewMatrix() * glm::vec4(lightPos, 1.0));
	m_lightProgram.update("light.Position", lightPosView);
	m_lightProgram.update("light.Color", lightColor);
	// Update attenuation parameters
	const float linear    = 0.09f;
	const float quadratic = 0.032f;
	m_lightProgram.update("enableSSAO", m_enableSSAO);
	m_lightProgram.update("light.Linear", linear);
	m_lightProgram.update("light.Quadratic", quadratic);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_gBuffer.gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_gBuffer.gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_gBuffer.gAlbedoSpec);
	glActiveTexture(GL_TEXTURE3); // add extra SSAO texture to lighting pass
	glBindTexture(GL_TEXTURE_2D, m_ssaoBuffer.ssaoColorBuffer);
	renderQuad();
}

void GLSSAOApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
	renderSSAOTexture(m_ssaoProgram, projection);
	renderBlurSSAOTexture(m_ssaoBlurProgram);
	renderLightPass(m_ssaoProgram);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable SSAO", &m_enableSSAO);
	ImGui::End();
}