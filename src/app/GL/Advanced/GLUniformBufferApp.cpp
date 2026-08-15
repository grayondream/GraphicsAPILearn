#include "GLUniformBufferApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

GLUniformBufferApp::~GLUniformBufferApp() {}

bool GLUniformBufferApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "UniformBuffer", "Cube.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "UniformBuffer", "Cube.frag");

	Cube cubeShape{};
	auto geo = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
	_vb = geo.vertexBuffer; _uv = geo.uvBuffer; _ebo = geo.indexBuffer;
	_indexCount = geo.indexCount;

	for(int i = 0;i < 4;i ++) {
		auto shader = renderer()->createShader();
		auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
		                           {rhi::ShaderStage::Fragment, ffile, "main", false}});
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto pipeline = renderer()->createPipeline(geo.layout, shader);
		pipeline->bindUniformBlock(0);
		_pipelines.push_back(pipeline);
	}

	_ubo = renderer()->createUniformBuffer();
	_ubo->init(nullptr, sizeof(glm::mat4) * 2, rhi::BufferType::Uniform);
	_ubo->bindRange(0, 0, sizeof(glm::mat4) * 2);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	_ubo->update(glm::value_ptr(projection), sizeof(glm::mat4), 0);
	_ubo->update(glm::value_ptr(view), sizeof(glm::mat4), sizeof(glm::mat4));
	return true;
}

void GLUniformBufferApp::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();
	const auto x = 1;
	glm::vec3 cubePositions[] = {
	  glm::vec3(-x, -x, -1.f),
	  glm::vec3(-x, x, -1.f),
	  glm::vec3(x, x, -1.f),
	  glm::vec3(x, -x, -1.f),
	};

	glm::vec4 colors[] = {
		glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
	};

	static float curTime = 0;
	curTime += dt;

	for (int i = 0; i < 4; i++) {
		renderer()->setPipeline(_pipelines[i]);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_uv, 1);
		renderer()->setIndexBuffer(_ebo);
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		_pipelines[i]->setUniform("model", glm::value_ptr(model), 1);
		_pipelines[i]->setUniform("cubeColor", glm::value_ptr(colors[i]), 1, 4);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

}
