#include "GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include "Geometry/Cube.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Utils/FileUtils.hpp"
#include "Utils/EventUtils.hpp"
#include "Base/Log.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

using FileUtils::join;
using namespace ErrorHandle;
using namespace Utils::Event;
GLCameraBaseApp::~GLCameraBaseApp() {

}

bool GLCameraBaseApp::initApp() {
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	return true;
}

void GLCameraBaseApp::onKeyPress(int key, int scancode, int action, int mods) {
	const auto keycode = ConvertKeyCode(key);
	switch (keycode) {
	case Key::W:
		_camera.processKeyboardEvent(Camera::Movement::Forward, 0.5); break;
	case Key::S:
		_camera.processKeyboardEvent(Camera::Movement::Backward, 0.5); break;
	case Key::D:
		_camera.processKeyboardEvent(Camera::Movement::Right, 0.5); break;
	case Key::A:
		_camera.processKeyboardEvent(Camera::Movement::Left, 0.5); break;
	default:
		break;
	}

	return GLApp::onKeyPress(key, scancode, action, mods);
}

void GLCameraBaseApp::onMouseButton(int button, int action, int mods) {
	const auto btn = ConvertMouseButton(button);
	const auto btnState = ConvertMouseAction(action);
	if (btnState == MouseAction::Press) {
		_mouseClicked = true;
	}else if (btnState == MouseAction::Release) {
		_mouseClicked = false;
	}
}

void GLCameraBaseApp::onMouseMove(double x, double y) {
	// 仅在鼠标被点击时处理移动事件
	if (!_mouseClicked) {
		_lastPos.x = x;
		_lastPos.y = y;
		return GLApp::onMouseMove(x, y);
	}

	// 计算偏移量
	const float offx = x - _lastPos.x;
	const float offy = y - _lastPos.y;

	// 更新摄像机
	_camera.processMouseMove(offx, offy);
	_lastPos.x = x;
	_lastPos.y = y;
	return GLApp::onMouseMove(x, y);
}

void GLCameraBaseApp::onMouseScroll(double xoffset, double yoffset) {
	_camera.processMouseScrool(yoffset);
	return GLApp::onMouseScroll(xoffset, yoffset);
}