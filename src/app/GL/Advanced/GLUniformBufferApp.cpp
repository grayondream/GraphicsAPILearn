#include "GLUniformBufferApp.hpp"
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

GLUniformBufferApp::~GLUniformBufferApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
	}

	for(auto& program : _programs) {
		program.destroy();
	}
}

bool GLUniformBufferApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto vfile = StaticCollector::getGLShaderPath() / "Advanced" / "UniformBuffer" / "Cube.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Advanced" / "UniformBuffer" / "Cube.frag";
	for(int i = 0;i < 4;i ++) {
		GLProgram program{};
		auto ret = program.init(vfile.string(), ffile.string());
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
		_programs.push_back(program);
	}

	createVertexBuffer();
	createUniformBuffer();
	return true;
}

void GLUniformBufferApp::createVertexBuffer() {
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

unsigned int GLUniformBufferApp::createUniformBuffer() {
	for(auto i = 0;i < 4;i ++) {
		auto program = _programs[i];
		program.uniformBind("Matrices", 0);
	}

	unsigned int uboMatrices;
	glGenBuffers(1, &uboMatrices);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * 2, nullptr, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
	return uboMatrices;
}

void GLUniformBufferApp::clearColor() {
	return GLApp::clearColor();
}

void GLUniformBufferApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLUniformBufferApp::drawScene(const float dt) {
	glBindVertexArray(_vao);
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
		auto program = _programs[i];
		program.use();
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		program.update("model", model);
		program.update("cubeColor", colors[i]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}

void GLUniformBufferApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLUniformBufferApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLUniformBufferApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLUniformBufferApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLUniformBufferApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLUniformBufferApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}