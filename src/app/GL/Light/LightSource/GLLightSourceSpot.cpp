#include "GLLightSourceSpot.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLLightSourceSpot::~GLLightSourceSpot() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_lightProgram.destroy();
	_targetProgram.destroy();
}

bool GLLightSourceSpot::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "LightSource", "Spot", "Light.vert");
		const auto ffile = join(shaderDir, "LightSource", "Spot", "Light.frag");
		auto ret = _lightProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "LightSource", "Spot", "Object.vert");
		const auto ffile = join(shaderDir, "LightSource", "Spot", "Object.frag");
		auto ret = _targetProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
	
	{
		const auto objImg = join(StaticCollector::getImagePath(), "container2.jpg");
		_objTex = std::make_shared<GLImageTexture2D>(objImg);
		const auto valid = _objTex->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", objImg);
	}

	{
		const auto objImg = join(StaticCollector::getImagePath(), "container2_specular.jpg");
		_objBorderTex = std::make_shared<GLImageTexture2D>(objImg);
		const auto valid = _objBorderTex->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", objImg);
	}

	createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLLightSourceSpot::createVertexBuffer() {
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

void GLLightSourceSpot::drawScene(const float dt) {
	GLApp::drawScene(dt);
	glBindVertexArray(_vao);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]); // ֧�� RGBA

	static int count{ 1 };
	ImGui::SetNextItemWidth(200);
	int cnt = 125;
	ImGui::SliderInt("Cube Count", &count, 1, cnt);
	ImGui::End();
	std::vector<glm::vec3> cubePositions;
	cubePositions.reserve(cnt);
	int gridSize = 5; // 5x5x5 = 125
	float spacing = 2.0f;
	glm::vec3 center(0.0f, 0.0f, 0.0f);

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

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f; // 旋转半径
	glm::vec3 lightPos = glm::vec3(
		radius * sin(curTime),
		0.0f,
		radius * cos(curTime)
	);
	//draw light source
	{
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, lightPos);
		model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
		model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));

		_lightProgram.use();
		_lightProgram.update("projection", projection);
		_lightProgram.update("view", view);
		_lightProgram.update("lightColor", _lightColor);
		_lightProgram.update("model", model);
		glDrawElements(GL_TRIANGLES, _object.idxSize(), GL_UNSIGNED_INT, 0);
	}

	//draw object
	{
		{
			_targetProgram.use();
			_targetProgram.update("projection", projection);
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
			_targetProgram.update("light.position", glm::vec4(lightPos, 1.0));
			_targetProgram.update("light.outerCutOff", glm::cos(glm::radians(17.5f)));
		}

		for (int i = 0; i < count; i++) {
			glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			model = glm::translate(model, cubePositions[i]);
			float angle =  20.0f * curTime;
			model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			_targetProgram.update("model", model);

			glDrawElements(GL_TRIANGLES, _object.idxSize(), GL_UNSIGNED_INT, 0);
		}
	}

	glBindVertexArray(0);
}
