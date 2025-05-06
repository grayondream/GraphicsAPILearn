#include "GLAdvancedGLSLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"

using namespace ErrorHandle;

GLAdvancedGLSLApp::~GLAdvancedGLSLApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
	}

	_program.destroy();
}

bool GLAdvancedGLSLApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto vfile = StaticCollector::getGLShaderPath() / "Advanced" / "GLSL" / "Cube.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Advanced" / "GLSL" / "Cube.frag";
	auto ret = _program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	const auto imgFile = StaticCollector::getImagePath() / "dog.jpg";
	_texture = std::make_shared<GLImageTexture2D>(imgFile.string());
	const auto valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile.string());
	createVertexBuffer();
	return true;
}

void GLAdvancedGLSLApp::createVertexBuffer() {
	Cube shape{};
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);
	}
	glBindVertexArray(0);
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
}

void GLAdvancedGLSLApp::clearColor() {
	return GLApp::clearColor();
}

void GLAdvancedGLSLApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLAdvancedGLSLApp::drawScene(const float dt) {
	glBindVertexArray(_vao);
	ImGui::Begin("OpenGL");
	static int count{ 1 };
	ImGui::Checkbox("Enable Point Size", &_enablePointSize);
    ImGui::Checkbox("Enable Frag Coord", &_enableFragCoord);
    ImGui::Checkbox("Enable Vertex Id", &_enableVertexId);
    ImGui::Checkbox("Enable Front Face Culling", &_enableFrontFaceCulling);
	ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();
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
    if(_enablePointSize){
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        //glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
        glEnable(GL_PROGRAM_POINT_SIZE);
    }else{
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_PROGRAM_POINT_SIZE);
        //glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    }

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		_program.update("model", model);
        _program.update("enablePointSize", _enablePointSize);
        _program.update("enableFragCoord", _enableFragCoord);
        _program.update("enableVertexId", _enableVertexId);
        _program.update("enableFrontFaceCulling", _enableFrontFaceCulling);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}

void GLAdvancedGLSLApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLAdvancedGLSLApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLAdvancedGLSLApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLAdvancedGLSLApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLAdvancedGLSLApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLAdvancedGLSLApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}