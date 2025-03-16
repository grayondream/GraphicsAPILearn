#include "GLLightSourceMult.hpp"
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

using namespace ErrorHandle;

GLLightSourceMult::~GLLightSourceMult() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}
}

bool GLLightSourceMult::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 5.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto shaderDir = StaticCollector::getGLShaderPath() / "Light";
	 initProgram("Light", _lightProgram);
	 initProgram("Object", _targetProgram);
	_objTex = initTexture("container2.jpg");
	_objBorderTex = initTexture("container2_specular.jpg");
	createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLLightSourceMult::initProgram(const std::string name, GLProgram &program) {
	const auto shaderDir = StaticCollector::getGLShaderPath() / "Light";
	const auto vfile = shaderDir / "LightSource" / "Mult" / std::string(name + ".vert");
	const auto ffile = shaderDir / "LightSource" / "Mult" / std::string(name + ".frag");
	LOGI("Generate program {}", name);
	LOGI("Vertex file : {}", vfile.string());
	LOGI("Fragment file : {}", ffile.string());
	auto ret = program.init(vfile.string(), ffile.string());
	ASSERT(ret, "Failed to create program {}", name);
}

std::shared_ptr<GLImageTexture2D> GLLightSourceMult::initTexture(const std::string img) {
	const auto imgfile = StaticCollector::getImagePath() / img;
	auto tex = std::make_shared<GLImageTexture2D>(imgfile.string());
	LOGI("Initalize texture from file {}", imgfile.string());
	const auto valid = tex->load().texture()->valid();
	ASSERT(valid, "Failed to initalize texture from {}", imgfile.string());
	return tex;
}

void GLLightSourceMult::createVertexBuffer() {
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, _object.byteSize(), _object.toGL().data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		// �󶨵ڶ��� VBO�����ö��㷨��
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, _object.normalSize(), _object.normal(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4DBase<float>), nullptr);
		glEnableVertexAttribArray(2); // ����

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, _object.uvSize(), _object.uv(), GL_STATIC_DRAW);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2DBase<float>), nullptr);
		glEnableVertexAttribArray(3);

		// ������������
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, _object.idxByteSize(), _object.idx(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);

	// ��¼ VBO �� EBO
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1], _vbo[2] = vbo[2];
	_ebo = ebo;
}

void GLLightSourceMult::clearColor() {
	return GLApp::clearColor();
}

void GLLightSourceMult::beginDrawScene() {
	return GLApp::beginDrawScene();
}

static std::vector<glm::vec3> GenerateCubePositions(const glm::vec3& center, int gridSize, float spacing) {
	std::vector<glm::vec3> cubePositions;
	cubePositions.reserve(gridSize * gridSize * gridSize);

	for (int i = 0; i < gridSize; i++) {
		for (int j = 0; j < gridSize; j++) {
			for (int k = 0; k < gridSize; k++) {
				glm::vec3 position(
					center.x + (i - gridSize / 2) * spacing,
					center.y + (j - gridSize / 2) * spacing,
					center.z + (k - gridSize / 2) * spacing - 10
				);
				cubePositions.push_back(position);
			}
		}
	}

	return cubePositions;
}

static int count{ 1 };
void GLLightSourceMult::drawUI() {
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]); // ֧�� RGBA

	ImGui::SetNextItemWidth(200);
	int cnt = 125;
	ImGui::SliderInt("Cube Count", &count, 1, cnt);
	ImGui::End();
}

void GLLightSourceMult::drawLight(const glm::mat4& proj, const glm::vec3& pos) {
	auto view = _camera.getViewMatrix();
	glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	model = glm::translate(model, pos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));

	_lightProgram.use();
	_lightProgram.update("projection", proj);
	_lightProgram.update("view", view);
	_lightProgram.update("lightColor", _lightColor);
	_lightProgram.update("model", model);
	glDrawElements(GL_TRIANGLES, _object.idxSize(), GL_UNSIGNED_INT, 0);
}

void GLLightSourceMult::drawObjects(const glm::mat4& proj, const float curTime, const glm::vec3& pos) {
	auto view = _camera.getViewMatrix();
	int gridSize = 5; // 5x5x5 = 125
	float spacing = 2.0f;
	glm::vec3 center(0.0f, 0.0f, 0.0f);
	std::vector<glm::vec3> cubePositions = GenerateCubePositions(center, gridSize, spacing);
	_targetProgram.use();
	_targetProgram.update("projection", proj);
	_targetProgram.update("view", view);
	const auto camPos = _camera.getAttr().pos;
	_targetProgram.update("viewPos", glm::vec4(camPos.x, camPos.y, camPos.z, 1.0));
	_targetProgram.update("material.shininess", 1);

	_objTex->texture()->bind(0);
	_targetProgram.update("material.diffuse", 0);
	_objBorderTex->texture()->bind(1);
	_targetProgram.update("material.specular", 1);

	glm::vec4 diffuseColor = glm::vec4(0.5); // 降低影响
	glm::vec4 ambientColor = glm::vec4(0.2f);
	_targetProgram.update("light.ambient", ambientColor);
	_targetProgram.update("light.diffuse", diffuseColor);
	_targetProgram.update("light.specular", glm::vec4(1.0f));
	_targetProgram.update("light.constant", 1.0f);
	_targetProgram.update("light.linear", 0.09f);
	_targetProgram.update("light.quadratic", 0.032f);
	_targetProgram.update("light.cutOff", glm::cos(glm::radians(12.5f)));
	auto attr = _camera.getAttr();
	_targetProgram.update("light.direction", glm::vec4(attr.front, 1.0));
	_targetProgram.update("light.position", glm::vec4(pos, 1.0));
	_targetProgram.update("light.outerCutOff", glm::cos(glm::radians(17.5f)));

	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		_targetProgram.update("model", model);

		glDrawElements(GL_TRIANGLES, _object.idxSize(), GL_UNSIGNED_INT, 0);
	}
}

void GLLightSourceMult::drawScene(const float dt) {
	GLApp::drawScene(dt);
	drawUI();
	glBindVertexArray(_vao);
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::vec3 lightPos = glm::vec3(5.0f * sin(curTime),0.0f, 5.0f * cos(curTime));
	drawLight(projection, lightPos);
	drawObjects(projection, curTime, lightPos);
	glBindVertexArray(0);
}

void GLLightSourceMult::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLLightSourceMult::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLLightSourceMult::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLLightSourceMult::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLLightSourceMult::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLLightSourceMult::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}