#include "GLTemplateTestApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include <Geometry/Plane.hpp>
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"

using namespace ErrorHandle;

GLTemplateTestApp::~GLTemplateTestApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}
}

static void CheckGLStencilAbility() {
	GLint stencilBits;
	glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
	LOGI("Stencil bits: {}", stencilBits);
	if (stencilBits == 0) {
		LOGI("Error: Stencil buffer is not available!");
		ExitIfFailed(false, "Stencil buffer is not available in this gl context");
	}
}

bool GLTemplateTestApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -10);
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	{
		const auto vfile = StaticCollector::getGLShaderPath() / "Advanced" / "TemplateTest" / "Basic.vert";
		const auto ffile = StaticCollector::getGLShaderPath() / "Advanced" / "TemplateTest" / "Basic.frag";
		auto ret = _program.init(vfile.string(), ffile.string());
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = StaticCollector::getGLShaderPath() / "Advanced" / "TemplateTest" / "Border.vert";
		const auto ffile = StaticCollector::getGLShaderPath() / "Advanced" / "TemplateTest" / "Border.frag";
		auto ret = _borderProgram.init(vfile.string(), ffile.string());
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto imgFile = StaticCollector::getImagePath() / "marble.jpg";
		_cubeTexture = std::make_shared<GLImageTexture2D>(imgFile.string());
		const auto valid = _cubeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile.string());
	}
	
	{
		const auto imgFile = StaticCollector::getImagePath() / "metal.jpg";
		_planeTexture = std::make_shared<GLImageTexture2D>(imgFile.string());
		const auto valid = _planeTexture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile.string());
	}

	createCubeBuffer();
	createPlaneBuffer();
	CheckGLStencilAbility();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	return true;
}

void GLTemplateTestApp::createCubeBuffer() {
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

void GLTemplateTestApp::createPlaneBuffer() {
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

void GLTemplateTestApp::clearColor() {
	return GLApp::clearColor();
}

void GLTemplateTestApp::beginDrawScene() {
	_cubeTexture->texture()->bind(0);
	_planeTexture->texture()->bind(1);
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

void GLTemplateTestApp::drawScene(const float dt) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	//ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();

	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size();

	// 设置投影矩阵
	_program.use();
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);

	// 获取视图矩阵
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);

	static float curTime = 0;
	curTime += dt;

	// 1. 清空模板缓冲和深度缓冲
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST); // 确保深度测试开启

	// 2. 绘制地面 (写入模板缓冲)
	{
		glStencilMask(0x00);
		glBindVertexArray(_planeVao);
		_program.use();
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-1.0, -4.50, -10));
		_program.update("model", model);
		_program.update("textureSampler", 1);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	// 3. 绘制多个立方体 (写入模板缓冲)
	{
		for (int i = 0; i < count; i++) {
			{
				glStencilFunc(GL_ALWAYS, 1, 0xFF); // 所有像素都写入模板缓冲，值为 1
				glStencilMask(0xFF); // 启用模板缓冲写入
				glBindVertexArray(_cubeVao);
				_program.use();
				_program.update("textureSampler", 0);
				auto model = glm::mat4(1.0f);
				model = glm::translate(model, cubePositions[i]);
				_program.update("model", model);
				glDrawArrays(GL_TRIANGLES, 0, 36);
				glBindVertexArray(0);
			}

			{
				glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // 仅在模板值不等于 1 时绘制
				glStencilMask(0x00); // 禁止写入模板缓冲
				glDisable(GL_DEPTH_TEST); // 禁用深度测试，确保描边在最前面
				_borderProgram.use();
				_borderProgram.update("view", view);
				_borderProgram.update("projection", projection);
				glBindVertexArray(_cubeVao);
				auto model = glm::mat4(1.0f);
				model = glm::translate(model, cubePositions[i]);
				const auto scale = 1.1f; // 稍微小一点的缩放
				model = glm::scale(model, glm::vec3(scale, scale, scale));
				_borderProgram.update("model", model);
				glDrawArrays(GL_TRIANGLES, 0, 36);
				glBindVertexArray(0);
				// 5. 重置 OpenGL 状态
				glStencilMask(0xFF);
				glStencilFunc(GL_ALWAYS, 0, 0xFF);
				glEnable(GL_DEPTH_TEST);
			}
		}
		
	}

	return GLApp::drawScene(dt);
}

void GLTemplateTestApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLTemplateTestApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLTemplateTestApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}

	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLTemplateTestApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}

	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLTemplateTestApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLTemplateTestApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}