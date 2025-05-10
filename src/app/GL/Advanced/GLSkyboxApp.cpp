
#include "GLSkyboxApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include "Geometry/Sphere.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Native/GL/GLImageTexture3D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Geometry/Image.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Utils/GL/GLAppUtils.hpp"
#include "Utils/FileUtils.hpp"
using FileUtils::join;
using namespace ErrorHandle;

GLSkyboxApp::~GLSkyboxApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(3, _vbo);
	}

	glDeleteVertexArrays(1, &_skyVao);
	glDeleteBuffers(1, &_skyVbo);
	_program.destroy();
}

static std::pair<unsigned int, unsigned int> CreateSkyBoxBuffer() {
	Cube shape{};
	unsigned int skyboxVAO, skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
	return { skyboxVAO, skyboxVBO };
}

bool GLSkyboxApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	_program = GLUtils::CompileShader("Advanced", "SkyBox", "Cube");
	_skyboxProgram = GLUtils::CompileShader("Advanced", "SkyBox", "SkyBox");
	const auto imgPath = StaticCollector::getImagePath();
	const auto imgFile = join(imgPath, "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	_skyBoxTexture = std::make_shared<GLImageTexture3D>(join(imgPath, "Skybox"));
	auto valid = _skyBoxTexture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);

	valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	createVertexBuffer();
	std::tie(_skyVao, _skyVbo) = CreateSkyBoxBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLSkyboxApp::createVertexBuffer() {
	Cube shape{};
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4DBase<float>), nullptr);
		glEnableVertexAttribArray(2); // ����

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(3);
	}
	glBindVertexArray(0);
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1], _vbo[2] = vbo[2];
}

void GLSkyboxApp::clearColor() {
	return GLApp::clearColor();
}

void GLSkyboxApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLSkyboxApp::drawCube() {
	glBindVertexArray(_vao);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);
	glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	model = glm::translate(model, glm::vec3(0, 0, 0));
	model = glm::rotate(model, glm::radians(0.f), glm::vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(0, 1, 0));
	_program.update("model", model);
	auto attr = _camera.getAttr();
	_program.update("cameraPos", attr.pos);
	_program.update("textureSampler", 0);
	_program.update("skyBoxSampler", 1);
	_program.update("enableReflection", _enableReflect);
	_program.update("enableRefraction", _enableRefraction);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void GLSkyboxApp::drawSkybox() {
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);  // 禁用深度写入
    
    _skyboxProgram.use();
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    _skyboxProgram.update("projection", projection);

    // 使用仅含旋转分量的视图矩阵
    auto view = glm::mat4(glm::mat3(_camera.getViewMatrix()));
    _skyboxProgram.update("view", view);
    
    _skyboxProgram.update("skybox", 1);
    glBindVertexArray(_skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthMask(GL_TRUE);   // 恢复深度写入
    glDepthFunc(GL_LESS);
}

void GLSkyboxApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Reflection", &_enableReflect);
	ImGui::Checkbox("Enable Refraction", &_enableRefraction);
	if (_enableReflect && _enableRefraction) {
		_enableReflect = _enableRefraction = false;
	}

	ImGui::End();
	_texture->texture()->bind(0);
	_skyBoxTexture->texture()->bind(1);
	
	drawCube();
	// 修改渲染顺序：先绘制天空盒，再绘制其他物体
	drawSkybox();
	
	return GLApp::drawScene(dt);
}

void GLSkyboxApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLSkyboxApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLSkyboxApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLSkyboxApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLSkyboxApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLSkyboxApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}