#include "GLLoadModelApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Base/Assert.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Model/Model.hpp"

using namespace ErrorHandle;

GLLoadModelApp::~GLLoadModelApp() {
}

bool GLLoadModelApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 5.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	initProgram("model", _program);
	loadModel();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLLoadModelApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	const auto modelFile = modelPath / "backpack.obj";
	_model = std::make_shared<Model>(modelFile.string().c_str());
}

void GLLoadModelApp::initProgram(const std::string name, GLProgram &program) {
	const auto shaderDir = StaticCollector::getGLShaderPath();
	const auto vfile = shaderDir / "Model" / std::string(name + ".vert");
	const auto ffile = shaderDir / "Model" / std::string(name + ".frag");
	LOGI("Generate program {}", name);
	LOGI("Vertex file : {}", vfile.string());
	LOGI("Fragment file : {}", ffile.string());
	auto ret = program.init(vfile.string(), ffile.string());
	ASSERT(ret, "Failed to create program {}", name);
}

void GLLoadModelApp::clearColor() {
	return GLApp::clearColor();
}

void GLLoadModelApp::beginDrawScene() {
	return GLApp::beginDrawScene();
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

void GLLoadModelApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLLoadModelApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		break;
	case WM_KEYUP:
		break;
	case WM_CHAR:
		const char ch = static_cast<char>(wParam);
		switch (wParam) {
		case 'w':
			_camera.processKeyboardEvent(Camera::Movement::Forward, 0.5); break;
		case 's':
			_camera.processKeyboardEvent(Camera::Movement::Backward, 0.5); break;
		case 'd':
			_camera.processKeyboardEvent(Camera::Movement::Right, 0.5); break;
		case 'a':
			_camera.processKeyboardEvent(Camera::Movement::Left, 0.5); break;
		}
		break;
	}

	return GLApp::onKeyBoardEvent(msg, wParam, lParam);
}

void GLLoadModelApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLLoadModelApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLLoadModelApp::onMouseMove(WPARAM btnState, int x, int y) {
	if (!_mouseClicked) {
		return GLApp::onMouseMove(btnState, x, y);
	}

	if (!_clicked) {
		_clicked = true;
		_lastPos = { (float)x, (float)y };
		return GLApp::onMouseMove(btnState, x, y);
	}

	const float offx = x - _lastPos.x;
	const float offy = y - _lastPos.y;
	_camera.processMouseMove(offx, offy);
	_lastPos = { (float)x, (float)y };
	return GLApp::onMouseMove(btnState, x, y);
}

void GLLoadModelApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}