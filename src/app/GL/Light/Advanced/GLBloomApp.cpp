#include "GLBloomApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Native/GL/GLCube.hpp"
#include "Native/GL/GLPlane.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLBloomApp::~GLBloomApp() {
	cube_->destroy();
	plane_->destroy();
}

void GLBloomApp::initShapes() {
	cube_ = std::make_shared< GLCube>();
	plane_ = std::make_shared< GLPlane>();

	cube_->init();
	plane_->init();
}

bool GLBloomApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	createTextures();
	compileShader();
	initShapes();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgname){
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
}

void GLBloomApp::createTextures(){
	wood_ = CreateTexture("wood.png");
	brick_ = CreateTexture("bricks2.jpg");
}

void GLBloomApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Bloom");
	{
		const auto vfile = join(shaderDir, "Bloom.vs");
		const auto ffile = join(shaderDir, "Bloom.fs");
		auto ret = _program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

void GLBloomApp::drawScene(const float dt) {
	auto pos = _camera.getAttr().pos;
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Hdr", &_enableHdr);
	ImGui::InputFloat("Exposure", &_exposure, 0.1f, 4.0f, "%.2f");
	ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);

	glm::vec3 cubePositions[] = {
	  glm::vec3(0.0f,  0.0f,  0.0f),
	  glm::vec3(2.0f,  5.0f, -15.0f),
	  glm::vec3(-1.5f, -2.2f, -2.5f),
	  glm::vec3(-3.8f, -2.0f, -12.3f),
	  glm::vec3(2.4f, -0.4f, -3.5f),
	  glm::vec3(-1.7f,  3.0f, -7.5f),
	  glm::vec3(1.3f, -2.0f, -2.5f),
	  glm::vec3(1.5f,  2.0f, -2.5f),
	  glm::vec3(1.5f,  0.2f, -1.5f),
	  glm::vec3(-1.3f,  1.0f, -1.5f)
	};

	glBindVertexArray(cube_->getVao());
	for (int i = 0; i < 10; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[i]);
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));
		projection = glm::perspective(glm::radians(45.0f), aspectRatio(), 0.1f, 100.0f);
		_program.update("model", model);
		_program.update("view", view);
		_program.update("projection", projection);
		wood_->texture()->bind(0);
		_program.update("textureSampler", 0);
		glDrawElements(GL_TRIANGLES, cube_->idxSize(), GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	ImGui::End();
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLBloomApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLBloomApp::onMouseMove(WPARAM btnState, int x, int y) {
	// 仅在鼠标被点击时处理移动事件
	if (!_mouseClicked) {
		return GLApp::onMouseMove(btnState, x, y);
	}

	// 计算偏移量
	const float offx = x - _lastx;
	const float offy = y - _lasty;

	// 更新摄像机
	_camera.processMouseMove(offx, offy);
	_lastx = x;
	_lasty = y;
	return GLApp::onMouseMove(btnState, x, y);
}

void GLBloomApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}