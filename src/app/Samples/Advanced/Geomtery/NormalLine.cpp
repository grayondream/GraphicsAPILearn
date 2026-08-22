#include "NormalLine.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "rhi/core/Common.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

NormalLine::~NormalLine() {
}

bool NormalLine::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}
	
	Sphere sphere{};
	auto geo = RhiGeometry::Create(renderer().get(), sphere, false, true, true, RhiGeometry::Layout{0, 2});
	_vb = geo.vertexBuffer; _normal = geo.normalBuffer; _ebo = geo.indexBuffer;
	_indexCount = geo.indexCount;

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "NormalLine.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "NormalLine.fs");
		const auto gfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "NormalLine.gs");

		auto shader = renderer()->createShader();
		auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
		                           {rhi::ShaderStage::Fragment, ffile, "main", false},
		                           {rhi::ShaderStage::Geometry, gfile, "main", false}});
		ErrorHandle::ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_normalPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "NormalLineSphere.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "NormalLineSphere.fs");
		auto shader = renderer()->createShader();
		auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
		                           {rhi::ShaderStage::Fragment, ffile, "main", false}});
		ErrorHandle::ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_pipeline = renderer()->createPipeline(geo.layout, shader);
	}
	
	_pipeline->setPolygonMode(rhi::PolygonMode::Line);
	_normalPipeline->setDepthTest(true);
	_pipeline->setDepthTest(true);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void NormalLine::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f;
	glm::vec3 pos = glm::vec3(0.0,0.0, -3.0f);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, pos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	const float scale = 2;
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	//draw light source
	{
		renderer()->setPipeline(_pipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->setIndexBuffer(_ebo);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	{
		renderer()->setPipeline(_normalPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->setIndexBuffer(_ebo);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
}