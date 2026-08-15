#include "GLMultieInstanceApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
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

GLMultieInstanceApp::~GLMultieInstanceApp() {
}

static std::shared_ptr<rhi::IBuffer> createObjectPositions(rhi::IRenderer* renderer, int count, int gap = 2.5){
	std::vector<glm::vec2> translations;
	translations.reserve(count * count);
	float offset = 0.1f;
	const int length = 2 * gap * (count - 1) / 2;
	for (int y = -length; y < length; y += 2 * gap)
	{
		for (int x = -length; x < length; x += 2 * gap)
		{
			glm::vec2 translation;
			translation.x = x;
			translation.y = y;
			translations.push_back(translation);
			LOGI("Append Position [{}, {}]", x, y);
		}
	}

	auto instanceVb = renderer->createBuffer();
	std::vector<glm::vec2> padded(translations);
	padded.resize((size_t)count * count);
	instanceVb->init(padded.data(), padded.size() * sizeof(padded[0]), rhi::BufferType::Vertex);
	return instanceVb;
}

bool GLMultieInstanceApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Sphere.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Sphere.fs");
	auto shader = renderer()->createShader();
	auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
	                           {rhi::ShaderStage::Fragment, ffile, "main", false}});
	ErrorHandle::ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	auto geo = RhiGeometry::Create(renderer().get(), shape, false, true, true, RhiGeometry::Layout{0, 2});
	_vb = geo.vertexBuffer; _normal = geo.normalBuffer; _ebo = geo.indexBuffer;
	_indexCount = geo.indexCount;
	_instanceVb = createObjectPositions(renderer().get(), _count);
	geo.layout.elements.push_back({rhi::VertexElement::Float2, 3, 3, rhi::VertexInputRate::PerInstance, 0, 8});

	_pipeline = renderer()->createPipeline(geo.layout, shader);
	_pipeline->setPolygonMode(rhi::PolygonMode::Line);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLMultieInstanceApp::draw(const float dt) {
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
	const float scale = 0.3;
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	{
		renderer()->setPipeline(_pipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->setVertexBuffer(_instanceVb, 3);
		renderer()->setIndexBuffer(_ebo);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetUniform(_ubo, "count", _count * _count);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->drawIndexedInstanced(_indexCount, (uint32_t)(_count * _count), 0, 0);
	}

}