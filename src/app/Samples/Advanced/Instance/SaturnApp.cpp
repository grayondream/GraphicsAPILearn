#include "SaturnApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "rhi/core/Common.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
#include <model/Model.hpp>
#include <cstddef>
using FileUtils::join;

using namespace ErrorHandle;

SaturnApp::~SaturnApp() {
}

std::vector<glm::mat4> GenerateRocksPosition(int amount, const glm::mat4& pos) {
	std::vector<glm::mat4> modelMatrices;
	modelMatrices.resize(amount);
	srand(static_cast<unsigned int>(0));
	float radius = 20.0;
	float offset = 10.0f;
	for (unsigned int i = 0; i < amount; i++)
	{
		glm::mat4 model = pos;
		float angle = (float)i / (float)amount * 360.0f;
		float displacement = (rand() % (int)(20 * offset * 10)) / 10.0f - offset;
		float x = sin(angle) * radius + displacement;
		displacement = (rand() % (int)(2 * offset * 10)) / 10.0f - offset;
		float y = displacement * 0.4f;
		displacement = (rand() % (int)(2 * offset * 10)) / 10.0f - offset;
		float z = cos(angle) * radius + displacement;
		model = glm::translate(model, glm::vec3(x, y, z));

		float scale = static_cast<float>((rand() % 20) / 100.0 + 0.05);
		model = glm::scale(model, glm::vec3(scale));

		float rotAngle = static_cast<float>((rand() % 360));
		model = glm::rotate(model, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

		modelMatrices[i] = model;
	}

	return modelMatrices;
}

std::shared_ptr<rhi::IBuffer> SaturnApp::generateRockInstanceBuffer(int count) {
	const auto poses = GenerateRocksPosition(count, glm::mat4(1.0f));
	auto buf = renderer()->createBuffer();
	buf->init(poses.data(), poses.size() * sizeof(poses[0]), rhi::BufferType::Vertex);
	return buf;
}

bool SaturnApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}
	
	_saturnPos = glm::vec3(0, 0, -3);
	loadModel();
	initSaturnPipeline();

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void SaturnApp::initSaturnPipeline() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.fs");
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ErrorHandle::ExitIfFailed(ok, "Create Saturn RHI shader failed: {}", shader->getLog());
	_saturnPipeline = renderer()->createPipeline(_saturn->vertexLayout(), shader);
	_saturnPipeline->setDepthTest(true);
}

void SaturnApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	{
		const auto modelFile = join(modelPath, "planet", "planet.obj");
		_saturn = std::make_shared<Model>(renderer().get(), modelFile);
	}

	{
		const auto modelFile = join(modelPath, "rock", "rock.obj");
		_rock = std::make_shared<Model>(renderer().get(), modelFile);
	}

	{
		_count = 30000;
		_instanceBuffer = generateRockInstanceBuffer(_count);
		const int stride = static_cast<int>(sizeof(MeshVertex));
		rhi::VertexLayout rockLayout;
		rockLayout.elements = {
			{ rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride },
			{ rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, Normal)), stride },
			{ rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, static_cast<int>(offsetof(MeshVertex, TexCoords)), stride },
			{ rhi::VertexElement::Float4, 3, 3, rhi::VertexInputRate::PerInstance, 0, static_cast<int>(sizeof(glm::mat4)) },
			{ rhi::VertexElement::Float4, 4, 3, rhi::VertexInputRate::PerInstance, static_cast<int>(sizeof(glm::vec4)), static_cast<int>(sizeof(glm::mat4)) },
			{ rhi::VertexElement::Float4, 5, 3, rhi::VertexInputRate::PerInstance, static_cast<int>(2 * sizeof(glm::vec4)), static_cast<int>(sizeof(glm::mat4)) },
			{ rhi::VertexElement::Float4, 6, 3, rhi::VertexInputRate::PerInstance, static_cast<int>(3 * sizeof(glm::vec4)), static_cast<int>(sizeof(glm::mat4)) },
		};
		const auto rvfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.vs");
		const auto rffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.fs");
		auto rshader = renderer()->createShader();
		auto rok = rshader->compile({ {rhi::ShaderStage::Vertex, rvfile, "main", false},
		                              {rhi::ShaderStage::Fragment, rffile, "main", false} });
		ErrorHandle::ExitIfFailed(rok, "Create Rock RHI shader failed: {}", rshader->getLog());
		_rockPipeline = renderer()->createPipeline(rockLayout, rshader);
		_rockPipeline->setDepthTest(true);
	}
}

void SaturnApp::draw(const float dt) {
	ImGui::Begin(rhi::backendDisplayName());
	ImGui::End();
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f;
	auto model = glm::translate(glm::mat4(1.0), _saturnPos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	const float scale = 0.3;
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	{
		renderer()->setPipeline(_saturnPipeline);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		model = glm::rotate(model, glm::radians(curTime * 5), glm::vec3(1.0, 1.0, 0.0));
		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		_saturn->draw(renderer().get(), _saturnPipeline.get());
	}

	{
		renderer()->setPipeline(_rockPipeline);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		model = glm::translate(model, glm::vec3(0.0f, 0.f, 0.0f));
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetUniform(_ubo, "time", curTime);
		rhi::SetUniform(_ubo, "radiusPos", _saturnPos);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		for (size_t i = 0; i < _rock->meshes.size(); i++) {
			const auto& mesh = _rock->meshes[i];
			for (size_t k = 0; k < mesh.textures.size(); k++) {
				renderer()->bindTexture(mesh.textures[k].texture, (unsigned int)k);
			}
			renderer()->setVertexBuffer(mesh.vertexBuffer());
			renderer()->setIndexBuffer(mesh.indexBuffer());
			renderer()->setVertexBuffer(_instanceBuffer, 3);
			renderer()->drawIndexedInstanced((uint32_t)mesh.indices.size(), (uint32_t)_count, 0, 0);
		}
	}

}