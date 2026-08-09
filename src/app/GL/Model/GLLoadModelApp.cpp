#include "GLLoadModelApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
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

bool GLLoadModelApp::initApp() {
	if (!GLApp::initApp()) {
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
}

void GLLoadModelApp::drawUI() {
	ImGui::Begin("OpenGL");
	ImGui::End();
}

void GLLoadModelApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	drawUI();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderer()->setPipeline(_pipeline);
	_pipeline->setUniform("projection", glm::value_ptr(projection), 1);
	_pipeline->setUniform("view", glm::value_ptr(view), 1);
	glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
	_pipeline->setUniform("model", glm::value_ptr(model), 1);
	_model->draw(renderer().get(), _pipeline.get());
}
