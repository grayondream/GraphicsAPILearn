#include "GLHdrApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLHdrApp::~GLHdrApp() {
}

static RhiGeometry::Geometry CreateCubeBuffer(rhi::IRenderer* renderer) {
	// 原始 36 顶点 cube：每顶点 8 float（pos3 + normal3 + uv2），与 GLHdrApp.cpp 原 CreateRectBuffer 相同
	float vertices[] = {
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
	};
	constexpr int stride = 8 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 24, stride});
	return RhiGeometry::CreateFromArray(renderer, vertices, sizeof(vertices), 36, layout);
}

static RhiGeometry::Geometry CreateQuadBuffer(rhi::IRenderer* renderer) {
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
	return RhiGeometry::CreateFromArray(renderer, quadVertices, sizeof(quadVertices), 4, layout);
}

bool GLHdrApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	auto cube = CreateCubeBuffer(renderer().get());
	_vb = cube.vertexBuffer;
	_cubeVertexCount = cube.vertexCount;
	const auto cubeLayout = cube.layout;

	auto quad = CreateQuadBuffer(renderer().get());
	_quadVb = quad.vertexBuffer;
	_quadVertexCount = quad.vertexCount;
	const auto quadLayout = quad.layout;

	compileShader(cubeLayout, quadLayout);

	const int w = static_cast<int>(m_window->getProperties().width);
	const int h = static_cast<int>(m_window->getProperties().height);
	_hdrRT = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!_hdrRT->create(fbd)) {
		ExitIfFailed(false, "Failed to create Hdr framebuffer");
	}
	_colorBuffer = std::shared_ptr<rhi::ITexture2D>(_hdrRT->colorTexture2D(0), [](rhi::ITexture2D*){});

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_brick = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_brick != nullptr, "Failed to load texture from file {}", imgFile);
	}
	return true;
}

void GLHdrApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Hdr");
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Lighting.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Lighting.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_objPipeline = renderer()->createPipeline(cubeLayout, shader);
		_objPipeline->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Hdr.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Hdr.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_hdrPipeline = renderer()->createPipeline(quadLayout, shader);
		_hdrPipeline->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
	}
}

static auto GetLightPosColor(){
	std::vector<glm::vec3> lightPositions;
	lightPositions.push_back(glm::vec3( 0.0f,  0.0f, 49.5f)); // back light
	lightPositions.push_back(glm::vec3(-1.4f, -1.9f, 9.0f));
	lightPositions.push_back(glm::vec3( 0.0f, -1.8f, 4.0f));
	lightPositions.push_back(glm::vec3( 0.8f, -1.7f, 6.0f));
	// colors
	std::vector<glm::vec3> lightColors;
	lightColors.push_back(glm::vec3(200.0f, 200.0f, 200.0f));
	lightColors.push_back(glm::vec3(0.1f, 0.0f, 0.0f));
	lightColors.push_back(glm::vec3(0.0f, 0.0f, 0.2f));
	lightColors.push_back(glm::vec3(0.0f, 0.1f, 0.0f));
	return std::pair(lightPositions, lightColors);
}

void GLHdrApp::render2FrameBuffer() {
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	renderer()->setRenderTarget(_hdrRT);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(_objPipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->bindTexture(_brick, 0);
	rhi::SetUniform(_ubo, "projection", projection);
	rhi::SetUniform(_ubo, "view", view);
	auto [lightPositions, lightColors] = GetLightPosColor();
	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		rhi::SetLight(_ubo, i, "Position", lightPositions[i]);
		rhi::SetLight(_ubo, i, "Color", lightColors[i]);
	}
	rhi::SetUniform(_ubo, "viewPos", _camera.getAttr().pos);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 25.0));
	model = glm::scale(model, glm::vec3(2.5f, 2.5f, 27.5f));
	rhi::SetUniform(_ubo, "model", model);
	rhi::SetUniform(_ubo, "inverse_normals", true);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_cubeVertexCount, 0);
	renderer()->setRenderTarget(nullptr);
}

void GLHdrApp::renderHdr() {
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(_hdrPipeline);
	renderer()->bindTexture(_hdrRT->colorTexture2D(0), 0);
	rhi::SetUniform(_ubo, "hdr", _enableHdr);
	rhi::SetUniform(_ubo, "exposure", _exposure);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->setVertexBuffer(_quadVb);
	renderer()->draw(_quadVertexCount, 0);
}

void GLHdrApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Hdr", &_enableHdr);
	ImGui::InputFloat("Exposure", &_exposure, 0.1f, 4.0f, "%.2f");
	ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", _camera.getAttr().pos.x, _camera.getAttr().pos.y, _camera.getAttr().pos.z);
	ImGui::End();

	render2FrameBuffer();
	renderHdr();
}
