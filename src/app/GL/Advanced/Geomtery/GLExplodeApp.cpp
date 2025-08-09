#include "GLExplodeApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

GLExplodeApp::~GLExplodeApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_program.destroy();
}

bool GLExplodeApp::initApp(){
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.fs");
	const auto gfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Explode.gs");

	GLProgram program{};
	auto ret = program.init(vfile, ffile, gfile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	_program = program;
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	createVertexBuffer();
	return true;
}

void GLExplodeApp::createVertexBuffer() {
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		// �󶨵ڶ��� VBO�����ö��㷨��
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4DBase<float>), nullptr);
		glEnableVertexAttribArray(2); // ����

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

void GLExplodeApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();

	glBindVertexArray(_vao);
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f; // 旋转半径
	glm::vec3 pos = glm::vec3(0.0,0.0, -3.0f);
	//draw light source
	{
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, pos);
		model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
		const float scale = 2;
		model = glm::scale(model, glm::vec3(scale, scale, scale));

		_program.use();
		_program.update("projection", projection);
		_program.update("view", view);
		_program.update("model", model);
		_program.update("time", curTime);
		glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
