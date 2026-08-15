#include "GLFrameBufferApp.hpp"
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
#include "geometry/Cube.hpp"
#include <geometry/Plane.hpp>
#include "geometry/Rect.hpp"
#include "utils/FileUtils.hpp"
using FileUtils::join;

using namespace ErrorHandle;

GLFrameBufferApp::~GLFrameBufferApp() {
}

std::shared_ptr<rhi::IPipeline> GLFrameBufferApp::compileShader(const std::string& name, const rhi::VertexLayout& layout) {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "FrameBuffer", (name + ".vert"));
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "FrameBuffer", (name + ".frag"));
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
	return renderer()->createPipeline(layout, shader);
}

void GLFrameBufferApp::compileShader(const rhi::VertexLayout& layout) {
	_contentPipeline = compileShader("Basic", layout);
	_screenPipeline = compileShader("Screen", layout);
}

void GLFrameBufferApp::loadTexture() {
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "container2.jpg");
		_cubeTexture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_cubeTexture != nullptr, "Failed to load texture from file {}", imgFile);
	}
	
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "metal.jpg");
		_planeTexture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_planeTexture != nullptr, "Failed to load texture from file {}", imgFile);
	}
}

void GLFrameBufferApp::initGLEnv() {
}

bool GLFrameBufferApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!Sample::load(rhiRenderer)) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -10);
	loadTexture();

	Cube cubeShape{};
	auto cubeGeo = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
	_cubeVb = cubeGeo.vertexBuffer; _cubeUv = cubeGeo.uvBuffer; _cubeEbo = cubeGeo.indexBuffer;
	_cubeIndexCount = cubeGeo.indexCount;

	Plane plane{};
	auto planeGeo = RhiGeometry::Create(renderer().get(), plane, true, false, false);
	_planeVb = planeGeo.vertexBuffer; _planeUv = planeGeo.uvBuffer;
	_planeVertexCount = planeGeo.vertexCount;

	Rect rect{};
	auto screenGeo = RhiGeometry::Create(renderer().get(), rect, true, false, true);
	_screenVb = screenGeo.vertexBuffer; _screenUv = screenGeo.uvBuffer; _screenEbo = screenGeo.indexBuffer;

	compileShader(cubeGeo.layout);
	createFrameBuffer();
	initGLEnv();
	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLFrameBufferApp::createFrameBuffer() {
	_screenFbo = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = windowWidth(); fbd.height = windowHeight();
	rhi::FramebufferAttachment color;
	color.type = rhi::AttachmentType::Color;
	color.format = rhi::TextureFormat::RGB8;
	color.minFilter = color.magFilter = rhi::TextureFilter::Linear;
	fbd.attachments.push_back(color);
	rhi::FramebufferAttachment depth;
	depth.type = rhi::AttachmentType::DepthStencil;
	depth.format = rhi::TextureFormat::Depth24Stencil8;
	fbd.attachments.push_back(depth);
	if (!_screenFbo->create(fbd)) ExitIfFailed(false, "Failed to create framebuffer");
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 1.1f;

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 2, z * spacing - 5));
			}
		}
	}
	return positions;
}

void GLFrameBufferApp::drawPlane() {
	renderer()->setPipeline(_contentPipeline);
	renderer()->bindTexture(_planeTexture, 0);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	rhi::SetUniform(_ubo, "projection", projection);

	const auto view = _camera.getViewMatrix();
	rhi::SetUniform(_ubo, "view", view);
	renderer()->setVertexBuffer(_planeVb);
	renderer()->setVertexBuffer(_planeUv, 1);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-1.0, -4.50, -10));
	rhi::SetUniform(_ubo, "model", model);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_planeVertexCount, 0);
}

void GLFrameBufferApp::drawCube() {
	renderer()->setPipeline(_contentPipeline);
	renderer()->bindTexture(_cubeTexture, 0);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	rhi::SetUniform(_ubo, "projection", projection);

	const auto view = _camera.getViewMatrix();
	rhi::SetUniform(_ubo, "view", view);

	renderer()->setVertexBuffer(_cubeVb);
	renderer()->setVertexBuffer(_cubeUv, 1);
	renderer()->setIndexBuffer(_cubeEbo);

	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size();
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[i]);

		float angle = 0;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));

		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->drawIndexed(_cubeIndexCount, 0, 0);
	}
}

void GLFrameBufferApp::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	const char* items[4] = { "None", "Inversion", "Gray", "Kernel"};
	
	if (ImGui::Combo("Cube Count", &_selectEffectType, items, IM_ARRAYSIZE(items))) {
		LOGI("Select Effect {}", std::string(items[_selectEffectType]));
    }
	ImGui::End();
	
	renderer()->setRenderTarget(_screenFbo);
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	drawCube();
	drawPlane();

	renderer()->setRenderTarget(nullptr);

	_screenPipeline->setDepthTest(false);
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	renderer()->setPipeline(_screenPipeline);
	renderer()->bindTexture(_screenFbo->colorTexture2D(0), 0);
	rhi::SetUniform(_ubo, "effectType", _selectEffectType);
	renderer()->setVertexBuffer(_screenVb);
	renderer()->setVertexBuffer(_screenUv, 1);
	renderer()->setIndexBuffer(_screenEbo);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(6, 0, 0);
	_screenPipeline->setDepthTest(true);
}
