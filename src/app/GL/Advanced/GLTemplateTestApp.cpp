#include "GLTemplateTestApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include "geometry/Cube.hpp"
#include <geometry/Plane.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "utils/FileUtils.hpp"
using FileUtils::join;
using namespace ErrorHandle;

GLTemplateTestApp::~GLTemplateTestApp() {}

bool GLTemplateTestApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -10);

	Cube cubeShape{};
	auto cg = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
	_cubeVb = cg.vertexBuffer; _cubeUv = cg.uvBuffer; _cubeEbo = cg.indexBuffer;
	_cubeIndexCount = cg.indexCount;

	Plane plane{};
	auto pg = RhiGeometry::Create(renderer().get(), plane, true, false, false);
	_planeVb = pg.vertexBuffer; _planeUv = pg.uvBuffer; _planeVertexCount = pg.vertexCount;

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Basic.vert");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Basic.frag");
		auto shader = renderer()->createShader();
		auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
		                           {rhi::ShaderStage::Fragment, ffile, "main", false}});
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_pipeline = renderer()->createPipeline(cg.layout, shader);
	}

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Border.vert");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Border.frag");
		auto shader = renderer()->createShader();
		auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
		                           {rhi::ShaderStage::Fragment, ffile, "main", false}});
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_borderPipeline = renderer()->createPipeline(cg.layout, shader);
	}

	_pipeline->setDepthTest(true);
	_pipeline->setDepthFunc(rhi::CompareFunc::Less);
	_pipeline->setStencilTest(true);
	_pipeline->setStencilFunc(rhi::CompareFunc::NotEqual, 1, 0xFF);
	_pipeline->setStencilOp(rhi::StencilOp::Keep, rhi::StencilOp::Replace, rhi::StencilOp::Replace);

	_cubeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "marble.jpg"));
	_planeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "metal.jpg"));
	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 2.f;

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 6, z * spacing - 10));
			}
		}
	}
	return positions;
}

void GLTemplateTestApp::drawScene(const float dt) {
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	renderer()->bindTexture(_cubeTexture, 0);
	renderer()->bindTexture(_planeTexture, 1);
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	//ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();

	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	static float curTime = 0;
	curTime += dt;

	{
		_pipeline->setStencilFunc(rhi::CompareFunc::Always, 1, 0xFF);
		_pipeline->setStencilMask(0x00);
		renderer()->setPipeline(_pipeline);
		renderer()->setVertexBuffer(_planeVb);
		renderer()->setVertexBuffer(_planeUv, 1);
		renderer()->bindTexture(_planeTexture, 0);
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-1.0, -4.50, -10));
		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->draw(_planeVertexCount, 0);
	}

	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_cubeVb);
	renderer()->setVertexBuffer(_cubeUv, 1);
	renderer()->setIndexBuffer(_cubeEbo);
	rhi::SetUniform(_ubo, "projection", projection);
	rhi::SetUniform(_ubo, "view", view);
	for (int i = 0; i < count; i++) {
		{
			_pipeline->setStencilFunc(rhi::CompareFunc::Always, 1, 0xFF);
			_pipeline->setStencilMask(0xFF);
			auto model = glm::mat4(1.0f);
			model = glm::translate(model, cubePositions[i]);
			model = glm::scale(model, glm::vec3(1));
			rhi::SetUniform(_ubo, "model", model);
			_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
			renderer()->drawIndexed(_cubeIndexCount, 0, 0);
		}

		{
			_pipeline->setStencilFunc(rhi::CompareFunc::NotEqual, 1, 0xFF);
			_pipeline->setStencilMask(0x00);
			_pipeline->setDepthTest(false);
			renderer()->setPipeline(_borderPipeline);
			renderer()->setVertexBuffer(_cubeVb);
			renderer()->setVertexBuffer(_cubeUv, 1);
			renderer()->setIndexBuffer(_cubeEbo);
			auto model = glm::mat4(1.0f);
			model = glm::translate(model, cubePositions[i]);
			model = glm::scale(model, glm::vec3(1.1f));
			rhi::SetUniform(_ubo, "model", model);
			rhi::SetUniform(_ubo, "projection", projection);
			rhi::SetUniform(_ubo, "view", view);
			_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
			renderer()->drawIndexed(_cubeIndexCount, 0, 0);
			renderer()->setPipeline(_pipeline);
			renderer()->setVertexBuffer(_cubeVb);
			renderer()->setVertexBuffer(_cubeUv, 1);
			renderer()->setIndexBuffer(_cubeEbo);
		}
	}

	_pipeline->setStencilMask(0xFF);
	_pipeline->setStencilFunc(rhi::CompareFunc::Always, 0, 0xFF);
	_pipeline->setDepthTest(true);

	return GLApp::drawScene(dt);
}
