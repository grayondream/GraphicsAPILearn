#include "GLTemplateTestApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "geometry/Cube.hpp"
#include <geometry/Plane.hpp>
#include "native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "utils/FileUtils.hpp"
using FileUtils::join;

using namespace ErrorHandle;

GLTemplateTestApp::~GLTemplateTestApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}

	_program.destroy();
	_borderProgram.destroy();
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

bool GLTemplateTestApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -10);
	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Basic.vert");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Basic.frag");
		auto ret = _program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Border.vert");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "TemplateTest", "Border.frag");
		auto ret = _borderProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

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

	createCubeBuffer();
	createPlaneBuffer();
	CheckGLStencilAbility();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
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

void GLTemplateTestApp::beginDrawScene() {
	_cubeTexture->texture()->bind(0);
	_planeTexture->texture()->bind(1);
	return GLApp::beginDrawScene();
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 2.f; // ��������Ϊ 2.0f��ʹ�������������һ��

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 6, z * spacing - 10));
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
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	{
		glEnable(GL_DEPTH_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
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

	glBindVertexArray(_cubeVao);
	for (int i = 0; i < count; i++) {
		{
			_program.use();
			glEnable(GL_DEPTH_TEST);
			glStencilFunc(GL_ALWAYS, 1, 0xFF);
			glStencilMask(0xFF);
			auto model = glm::mat4(1.0f);
			model = glm::translate(model, cubePositions[i]);
			model = glm::scale(model, glm::vec3(1));
			_program.update("model", model);
			_program.update("textureSampler", 0);
			glDrawArrays(GL_TRIANGLES, 0, 36); // 确保使用正确的顶点数量
		}

		{
			glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
			glStencilMask(0x00);
			glDisable(GL_DEPTH_TEST);

			_borderProgram.use();
			auto model = glm::mat4(1.0f);
			model = glm::translate(model, cubePositions[i]);
			model = glm::scale(model, glm::vec3(1.1f)); // 增大缩放比例
			_borderProgram.update("model", model);
			_borderProgram.update("projection", projection);
			_borderProgram.update("view", view);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
	}
		
	glBindVertexArray(0);
	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glEnable(GL_DEPTH_TEST);

	return GLApp::drawScene(dt);
}
