#include "GLSimpleLightAmbination.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLSimpleLightAmbination::~GLSimpleLightAmbination() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_lightProgram.destroy();
	_targetProgram.destroy();
}

bool GLSimpleLightAmbination::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "Ambination", "Source.vert");
		const auto ffile = join(shaderDir, "Ambination", "Source.frag");
		auto ret = _lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Ambination", "Object.vert");
		const auto ffile = join(shaderDir, "Ambination", "Object.frag");
		auto ret = _targetProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
	
	createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLSimpleLightAmbination::createVertexBuffer() {
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

void GLSimpleLightAmbination::drawScene(const float dt) {
	GLApp::drawScene(dt);
	glBindVertexArray(_vao);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]); // ֧�� RGBA
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	
	const auto view = _camera.getViewMatrix();
	

	//draw object
	{
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));

		_targetProgram.use();
		_targetProgram.update("projection", projection);
		_targetProgram.update("view", view);
		_targetProgram.update("model", model);
		_targetProgram.update("lightColor", _lightColor);
		_targetProgram.update("objectColor", glm::vec4(1.0f, 0.5f, 0.31f, 1.0));
		glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
	}

	//draw light source
	{
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, glm::vec3(1.0f, 1.0f, 1.0f));
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));

		_lightProgram.use();
		_lightProgram.update("projection", projection);
		_lightProgram.update("view", view);
		_lightProgram.update("model", model);
		_lightProgram.update("lightColor", _lightColor);
		glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
}
