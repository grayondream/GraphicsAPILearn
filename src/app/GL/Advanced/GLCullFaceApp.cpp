#include "GLCullFaceApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include <Geometry/Plane.hpp>
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Utils/FileUtils.hpp"
using FileUtils::join;

using namespace ErrorHandle;

GLCullFaceApp::~GLCullFaceApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}

	_program.destroy();
}

bool GLCullFaceApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	initGLEnv();
	compileShader();
	createTexture();
	createCubeBuffer();
	createPlaneBuffer();
	
	return true;
}

void GLCullFaceApp::initGLEnv() {
	_camera = Camera(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, 5);
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void GLCullFaceApp::compileShader() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "CullFace", "Basic.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "CullFace", "Basic.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
}

void GLCullFaceApp::createTexture() {
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "marble.jpg");
		_cubeTexture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _cubeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "metal.jpg");
		_planeTexture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _planeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "grass.png");
		_grassTexture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _grassTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "window.png");
		_winTexture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _winTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}
}

void GLCullFaceApp::createCubeBuffer() {
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
	_cubeVao = vao;
	_cubeVbo[0] = vbo[0], _cubeVbo[1] = vbo[1];
}

void GLCullFaceApp::createPlaneBuffer() {
	Plane shape{};
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
	_planeVao = vao;
	_planeVbo[0] = vbo[0], _planeVbo[1] = vbo[1];
}

void GLCullFaceApp::clearColor() {
	return GLApp::clearColor();
}

void GLCullFaceApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 1.1f; // ��������Ϊ 2.0f��ʹ�������������һ��

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 2, z * spacing - 5));
			}
		}
	}
	return positions;
}

void GLCullFaceApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	ImGui::SliderInt("Grass Count", &_grassCount, 1, 10);
	ImGui::DragFloat3("Position", &_objectPosition[0], 0.1f);
	ImGui::DragFloat3("Scale", &_objectScale[0], 0.1f);
	ImGui::DragFloat3("Windows Pos", &_winPos[0], 0.1f);
	ImGui::End();
	
	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size(); 
	_cubeTexture->texture()->bind(0);
	
	_program.use();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);

	const auto view = _camera.getViewMatrix();
	_program.update("view", view);

	static float curTime = 0; 
	curTime += dt; 
	glBindVertexArray(_cubeVao);
	//_program.update("textureSampler", 0);

		glm::mat4 model = glm::mat4(1.0f); 
		model = glm::translate(model, glm::vec3(0, 0, 0) + _objectPosition);

		model = glm::rotate(model, glm::radians(45.f), glm::vec3(1, 0, 0)); 
		model = glm::rotate(model, glm::radians(45.f), glm::vec3(0, 1, 0));
		model = glm::scale(model, glm::vec3(2.0));
		_program.update("textureSampler", 0);
		_program.update("texColor", glm::vec4(1.0, 1.0, 1.0, 0.0));
		_program.update("model", model); // ����ģ�;���
		glDrawArrays(GL_TRIANGLES, 0, 36); // ����������

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}

void GLCullFaceApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLCullFaceApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLCullFaceApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}

	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLCullFaceApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}

	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLCullFaceApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLCullFaceApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}