#include "GLLoadModelApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "native/GL/GLImageTexture2D.hpp"
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
	_program.destroy();
}

bool GLLoadModelApp::initApp() {
	if (!GLApp::initApp()) {
		return false;
	}
	
	initProgram("model", _program);
	loadModel();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLLoadModelApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	const auto modelFile = join(modelPath, "backpack", "backpack.obj");
	_model = std::make_shared<Model>(modelFile);
}

void GLLoadModelApp::initProgram(const std::string name, GLProgram &program) {
	const auto shaderDir = StaticCollector::getGLShaderPath();
	const auto vfile = join(shaderDir, "Model", std::string(name + ".vert"));
	const auto ffile = join(shaderDir, "Model", std::string(name + ".frag"));
	LOGI("Generate program {}", name);
	LOGI("Vertex file : {}", vfile);
	LOGI("Fragment file : {}", ffile);
	auto ret = program.init(vfile, ffile);
	ASSERT(ret, "Failed to create program {}", name);
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
	_program.use();
	_program.update("projection", projection);
	_program.update("view", view);
	glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
	_program.update("model", model);
	_model->draw(_program);
}