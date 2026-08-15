#include "GLSSAOApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include "base/Log.hpp"
#include "imgui.h"
#include "utils/FileUtils.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Plane.hpp"


using FileUtils::join;
using namespace ErrorHandle;

GLSSAOApp::~GLSSAOApp() {
}

void GLSSAOApp::initShapes() {
    Cube cube{};
    // GBuffer.vs 需要 interleaved pos3+normal3+uv2（stride 8 floats），layout normal@1/uv@2
    const auto vtx = cube.data();       // Vertex* → float*：pos4+color4 交错，stride 8 floats
    const auto nrm = cube.normal();     // vec4 → float*
    const auto uv  = cube.uv();         // vec2 → float*
    const int count = static_cast<int>(cube.size());
    std::vector<float> interleaved;
    interleaved.reserve(static_cast<size_t>(count) * 8);
    for (int i = 0; i < count; ++i) {
        interleaved.push_back(vtx[i * 8 + 0]);
        interleaved.push_back(vtx[i * 8 + 1]);
        interleaved.push_back(vtx[i * 8 + 2]);
        interleaved.push_back(nrm[i * 4 + 0]);
        interleaved.push_back(nrm[i * 4 + 1]);
        interleaved.push_back(nrm[i * 4 + 2]);
        interleaved.push_back(uv[i * 2 + 0]);
        interleaved.push_back(uv[i * 2 + 1]);
    }
    constexpr int stride = 8 * static_cast<int>(sizeof(float));
    rhi::VertexLayout layout;
    layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
    layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
    layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 24, stride});
    auto geo = RhiGeometry::CreateFromArray(renderer().get(), interleaved.data(),
                                            interleaved.size() * sizeof(float), static_cast<uint32_t>(count), layout);
    m_cubeVb = geo.vertexBuffer;
    m_cubeVertexCount = geo.vertexCount;
    m_cubeLayout = geo.layout;
}

static std::vector<glm::vec3> GenerateSSAONoise(int kernelSize = 64) {
	std::vector<glm::vec3> ssaoNoise;
	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    for (unsigned int i = 0; i < kernelSize; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    return ssaoNoise;
}

void GLSSAOApp::createSSAOFbo() {
	const int w = static_cast<int>(windowWidth());
	const int h = static_cast<int>(windowHeight());
	m_ssaoBuffer.fbo = renderer()->createRenderTarget();
	rhi::FramebufferDesc ssaoDesc;
	ssaoDesc.width = w; ssaoDesc.height = h;
	ssaoDesc.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::R32F, false, 0,
	                                rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest});
	if (!m_ssaoBuffer.fbo->create(ssaoDesc)) ExitIfFailed(false, "Failed to create SSAO framebuffer");
	m_ssaoBuffer.ssaoColorBuffer = m_ssaoBuffer.fbo->colorTexture2D(0);

	m_ssaoBuffer.blurFbo = renderer()->createRenderTarget();
	rhi::FramebufferDesc blurDesc;
	blurDesc.width = w; blurDesc.height = h;
	blurDesc.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::R32F, false, 0,
	                                rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest});
	if (!m_ssaoBuffer.blurFbo->create(blurDesc)) ExitIfFailed(false, "Failed to create SSAO blur framebuffer");
	m_ssaoBuffer.ssaoBlurBuffer = m_ssaoBuffer.blurFbo->colorTexture2D(0);

	{
		const auto ssaoNoise = GenerateSSAONoise();
		auto noiseTex = renderer()->createTexture2D();
		rhi::TextureDesc noiseDesc;
		noiseDesc.format = rhi::TextureFormat::RGBA32F;
		noiseDesc.wrapS = noiseDesc.wrapT = rhi::TextureWrap::Repeat;
		noiseDesc.minFilter = rhi::TextureFilter::Nearest;
		noiseDesc.magFilter = rhi::TextureFilter::Nearest;
		noiseDesc.generateMipmap = false;
		rhi::TextureDataView2D noiseView{ssaoNoise.data(), 4, 4, 3};
		if (!noiseTex->init(noiseDesc, noiseView)) ExitIfFailed(false, "Failed to upload SSAO noise texture");
		m_ssaoBuffer.noiseTexture = noiseTex;
	}
}

void GLSSAOApp::createFrameBuffers() {
	createGBufferFbo();
	createSSAOFbo();
}

void GLSSAOApp::createGBufferFbo() {
	const int w = static_cast<int>(windowWidth());
	const int h = static_cast<int>(windowHeight());
	m_gBuffer.gbuffer = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA8, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!m_gBuffer.gbuffer->create(fbd)) ExitIfFailed(false, "Failed to create GBuffer framebuffer");
	m_gBuffer.gPosition = m_gBuffer.gbuffer->colorTexture2D(0);
	m_gBuffer.gNormal = m_gBuffer.gbuffer->colorTexture2D(1);
	m_gBuffer.gAlbedoSpec = m_gBuffer.gbuffer->colorTexture2D(2);
}

void GLSSAOApp::createQuadBuffer() {
	// 全屏 quad：pos(vec3)@0 + uv(vec2)@1，4 顶点交错（TriangleStrip），匹配 SSAO.vs
	float quadVertices[] = {
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	constexpr int stride = 5 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	auto geo = RhiGeometry::CreateFromArray(renderer().get(), quadVertices, sizeof(quadVertices), 4, layout);
	m_quadVb = geo.vertexBuffer;
	m_quadVertexCount = geo.vertexCount;
	m_quadLayout = geo.layout;
}

bool GLSSAOApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) return false;
	loadModel();
	initShapes();
	createTextures();
	createQuadBuffer();
	createFrameBuffers();
	compileShader(m_cubeLayout, m_quadLayout);
	initModelPipeline();
	return true;
}

void GLSSAOApp::createTextures() {

}

void GLSSAOApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	m_uboBuffer = renderer()->createUniformBuffer();
	m_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	m_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "SSAO");
	auto build = [&](const std::string& vs, const std::string& fs, const rhi::VertexLayout& layout, bool depthTest, rhi::PrimitiveType prim) {
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, vs), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, fs), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto pipe = renderer()->createPipeline(layout, shader);
		pipe->setPrimitiveType(prim);
		if (depthTest) pipe->setDepthTest(true);
		return pipe;
	};
	m_gBufferProgram = build("GBuffer.vs", "GBuffer.fs", cubeLayout, true, rhi::PrimitiveType::TriangleList);
	m_ssaoProgram = build("SSAO.vs", "SSAO.fs", quadLayout, false, rhi::PrimitiveType::TriangleStrip);
	m_ssaoBlurProgram = build("SSAOBlur.vs", "SSAOBlur.fs", quadLayout, false, rhi::PrimitiveType::TriangleStrip);
	m_lightProgram = build("Light.vs", "Light.fs", quadLayout, false, rhi::PrimitiveType::TriangleStrip);
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
	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
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
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}

void GLSSAOApp::renderOneCube() {
	renderer()->setVertexBuffer(m_cubeVb);
	renderer()->draw(m_cubeVertexCount, 0);
}

void GLSSAOApp::renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection, const glm::mat4& view) {
	renderer()->setRenderTarget(m_gBuffer.gbuffer);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(program);
	rhi::SetUniform(m_ubo, "projection", projection);
	rhi::SetUniform(m_ubo, "view", view);
	// room cube
	auto model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 7.0f, 0.0f)), glm::vec3(15.0f));  // 见下注
	rhi::SetUniform(m_ubo, "model", model);
	rhi::SetUniform(m_ubo, "invertedNormals", 1);
	m_uboBuffer->update(&m_ubo, sizeof(rhi::UniformBlock), 0);
	renderOneCube();
	rhi::SetUniform(m_ubo, "invertedNormals", 0);
	// backpack
	auto model2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0));
	model2 = glm::rotate(model2, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
	renderer()->setPipeline(m_modelPipeline);
	rhi::SetUniform(m_ubo, "model", model2);
	m_uboBuffer->update(&m_ubo, sizeof(rhi::UniformBlock), 0);
	m_model->draw(renderer().get(), m_modelPipeline.get());
	renderer()->setRenderTarget(nullptr);
}

void GLSSAOApp::renderSSAOTexture(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection){
	renderer()->setPipeline(program);
	renderer()->bindTexture(m_gBuffer.gPosition, 0);
	renderer()->bindTexture(m_gBuffer.gNormal, 1);
	renderer()->bindTexture(m_ssaoBuffer.noiseTexture, 2);
	const auto ssaoKernel = GenerateSSAOKernel();
	for (unsigned int i = 0; i < 64; ++i)
		rhi::SetUniform(m_ubo, "samples", i, ssaoKernel[i]);
	rhi::SetUniform(m_ubo, "projection", projection);
	renderer()->setRenderTarget(m_ssaoBuffer.fbo);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	m_uboBuffer->update(&m_ubo, sizeof(rhi::UniformBlock), 0);
	renderQuad();
	renderer()->setRenderTarget(nullptr);
}

void GLSSAOApp::renderBlurSSAOTexture(std::shared_ptr<rhi::IPipeline>& program){
	renderer()->setPipeline(program);
	renderer()->bindTexture(m_ssaoBuffer.ssaoColorBuffer, 0);
	renderer()->setRenderTarget(m_ssaoBuffer.blurFbo);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderQuad();
	renderer()->setRenderTarget(nullptr);
}

void GLSSAOApp::renderLightPass(std::shared_ptr<rhi::IPipeline>& program){
	renderer()->setPipeline(program);
	renderer()->bindTexture(m_gBuffer.gPosition, 0);
	renderer()->bindTexture(m_gBuffer.gNormal, 1);
	renderer()->bindTexture(m_gBuffer.gAlbedoSpec, 2);
	renderer()->bindTexture(m_ssaoBuffer.ssaoColorBuffer, 3);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	const glm::vec3 lightPos = glm::vec3(2.0, 4.0, -2.0);
	const glm::vec3 lightColor = glm::vec3(0.2, 0.2, 0.7);
	glm::vec3 lightPosView = glm::vec3(_camera.getViewMatrix() * glm::vec4(lightPos, 1.0));
	rhi::SetLight(m_ubo, 0, "Position", lightPosView);
	rhi::SetLight(m_ubo, 0, "Color", lightColor);
	const float linear = 0.09f, quadratic = 0.032f;
	rhi::SetUniform(m_ubo, "enableSSAO", m_enableSSAO ? 1 : 0);
	rhi::SetLightParam(m_ubo, 0, "Linear", linear);
	rhi::SetLightParam(m_ubo, 0, "Quadratic", quadratic);
	m_uboBuffer->update(&m_ubo, sizeof(rhi::UniformBlock), 0);
	renderQuad();
}

void GLSSAOApp::draw(const float dt) {
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
	renderSSAOTexture(m_ssaoProgram, projection);
	renderBlurSSAOTexture(m_ssaoBlurProgram);
	renderLightPass(m_lightProgram);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable SSAO", &m_enableSSAO);
	ImGui::End();
}