#include "GLBlinnPhongApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Geometry/Plane.hpp"
#include <Utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLBlinnPhongApp::~GLBlinnPhongApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_lightProgram.destroy();
	_targetProgram.destroy();
}

bool GLBlinnPhongApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Source.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Source.fs");
		auto ret = _lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Object.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Object.fs");
		auto ret = _targetProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
	
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _texture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	createVertexBuffer();
	createPlaneBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLBlinnPhongApp::createVertexBuffer() {
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		// �󶨵�һ�� VBO�����ö���λ��
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		// ������������
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);

	// ��¼ VBO �� EBO
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
	_ebo = ebo;
}

void GLBlinnPhongApp::createPlaneBuffer() {
	Plane shape{};
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
	}
	glBindVertexArray(0);
	_planeVao = vao;
	_planeVbo[0] = vbo[0], _planeVbo[1] = vbo[1];
	_planeVbo[2] = vbo[2];
}

void GLBlinnPhongApp::clearColor() {
	return GLApp::clearColor();
}

void GLBlinnPhongApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLBlinnPhongApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	glBindVertexArray(_vao);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]); // ֧�� RGBA
	ImGui::Checkbox("Enable Blinn Phong", &_enableBlinnPhong);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	
	const auto view = _camera.getViewMatrix();
	
	const auto lightPos = glm::vec3(-0.0f, 0.0f, 0.5f);
	//draw object
	{
		_texture->texture()->bind(0);
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		
		model = glm::translate(model, lightPos);
		//model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		model = glm::scale(model, glm::vec3(10.f));
		_targetProgram.use();
		glBindVertexArray(_planeVao);
		_targetProgram.update("projection", projection);
		_targetProgram.update("view", view);
		_targetProgram.update("model", model); // ����ģ�;���
		_targetProgram.update("textureSampler", 0);
		_targetProgram.update("lightColor", _lightColor);
		_targetProgram.update("lightPos", lightPos);
		_targetProgram.update("viewPos", _camera.getAttr().pos);
		_targetProgram.update("enableBlinnPhong", _enableBlinnPhong);
		glDrawArrays(GL_TRIANGLES, 0, 6); // ����������
		glBindVertexArray(0);
	}

	//draw light source
	{
		glBindVertexArray(_vao);
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		_lightProgram.use();
		_lightProgram.update("projection", projection);
		_lightProgram.update("view", view);
		_lightProgram.update("model", model);
		_lightProgram.update("lightColor", _lightColor);
		glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}

void GLBlinnPhongApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLBlinnPhongApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLBlinnPhongApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLBlinnPhongApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLBlinnPhongApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLBlinnPhongApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}