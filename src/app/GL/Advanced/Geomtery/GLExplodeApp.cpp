#include "GLExplodeApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

GLExplodeApp::~GLExplodeApp() {
}

bool GLExplodeApp::initApp(){
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.fs");
	const auto gfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.gs");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
	                           {rhi::ShaderStage::Fragment, ffile, "main", false},
	                           {rhi::ShaderStage::Geometry, gfile, "main", false}});
	ErrorHandle::ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
	Sphere sphere{};
	auto geo = RhiGeometry::Create(renderer().get(), sphere, false, true, true, RhiGeometry::Layout{0, 2});
	_vb = geo.vertexBuffer; _normal = geo.normalBuffer; _ebo = geo.indexBuffer;
	_indexCount = geo.indexCount;
	_pipeline = renderer()->createPipeline(geo.layout, shader);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLExplodeApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f;
	glm::vec3 pos = glm::vec3(0.0,0.0, -3.0f);
	//draw light source
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, pos);
		model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
		const float scale = 2;
		model = glm::scale(model, glm::vec3(scale, scale, scale));

		renderer()->setPipeline(_pipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->setIndexBuffer(_ebo);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetUniform(_ubo, "time", curTime);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	return GLApp::drawScene(dt);
}