#include "GLLoadModelApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <base/Assert.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "model/Model.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLLoadModelApp::~GLLoadModelApp() {
}

bool GLLoadModelApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!Sample::load(rhiRenderer)) {
		return false;
	}
	
	loadModel();
	initShader();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLLoadModelApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	const auto modelFile = join(modelPath, "backpack", "backpack.obj");
	_model = std::make_shared<Model>(renderer().get(), modelFile);
}

void GLLoadModelApp::initShader() {
	const auto shaderDir = StaticCollector::getGLShaderPath();
	const auto vfile = join(shaderDir, "Model", "model.vert");
	const auto ffile = join(shaderDir, "Model", "model.frag");
	LOGI("Compile shader {}", "model");
	LOGI("Vertex file : {}", vfile);
	LOGI("Fragment file : {}", ffile);

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ASSERT(ok, "Failed to create shader {}: {}", "model", shader->getLog());

	_pipeline = renderer()->createPipeline(_model->vertexLayout(), shader);
	_pipeline->setDepthTest(true);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
}

void GLLoadModelApp::drawUI() {
	ImGui::Begin("OpenGL");
	ImGui::End();
}

void GLLoadModelApp::draw(const float dt) {
	drawUI();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderer()->setPipeline(_pipeline);
	rhi::SetUniform(_ubo, "projection", projection);
	rhi::SetUniform(_ubo, "view", view);
	glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
	rhi::SetUniform(_ubo, "model", model);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	_model->draw(renderer().get(), _pipeline.get());
}