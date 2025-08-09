#include "GLBlendApp.hpp"
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

GLBlendApp::~GLBlendApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}

	_program.destroy();
}

bool GLBlendApp::initApp() {
	if (!GLApp::initApp()) {
		return false;
	}

	initGLEnv();
	compileShader();
	createTexture();
	createCubeBuffer();
	createPlaneBuffer();
	
	return true;
}

void GLBlendApp::initGLEnv() {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void GLBlendApp::compileShader() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Blend", "Basic.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Blend", "Basic.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
}

void GLBlendApp::createTexture() {
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

void GLBlendApp::createCubeBuffer() {
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

void GLBlendApp::createPlaneBuffer() {
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

void GLBlendApp::drawScene(const float dt) {
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
	_planeTexture->texture()->bind(1);
	_grassTexture->texture()->bind(2);
	_winTexture->texture()->bind(3);
	_program.use();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);

	const auto view = _camera.getViewMatrix();
	_program.update("view", view);

	static float curTime = 0; 
	curTime += dt; 
	glBindVertexArray(_cubeVao);
	//_program.update("textureSampler", 0);
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); 
		model = glm::translate(model, cubePositions[i] + _objectPosition);

		// ʹ�õ�ǰʱ�������������������ת�Ƕ�
		float angle = 0; // ÿ���������Բ�ͬ���ٶ���ת
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f)); // ��ת
		_program.update("textureSampler", 1);
		_program.update("texColor", glm::vec4(1.0, 1.0, 1.0, 0.0));
		_program.update("model", model); // ����ģ�;���
		glDrawArrays(GL_TRIANGLES, 0, 36); // ����������
	}

	glBindVertexArray(_planeVao);
	{
		glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
		model = glm::translate(model, glm::vec3(-1.0, -4.50, -10)); // ƽ����������λ��
		model = glm::scale(model, _objectScale);
		_program.update("model", model); // ����ģ�;���
		_program.update("textureSampler", 0);
		_program.update("texColor", glm::vec4(1.0, 1.0, 1.0, 0.0));
		glDrawArrays(GL_TRIANGLES, 0, 6); // ����������
	}

	{
		glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
		model = glm::translate(model, glm::vec3(0.5f, 0.5f, 0.5f)); // ƽ����������λ��
		model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		_program.update("model", model); // ����ģ�;���
		_program.update("textureSampler", 2);
		_program.update("texColor", glm::vec4(1.0, 1.0, 1.0, 0.0));
		glDrawArrays(GL_TRIANGLES, 0, 6); // ����������
	}

	{
		for (int i = 0; i < _grassCount; i++) {
			glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
			model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5));
			model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			//如果這裏的坐標是亂序的，則需要根據距離排序透明目標
			//但是這裏已經是有序的了就不做了
			model = glm::translate(model, _winPos + glm::vec3(-0.5 * i, 1 * i, 0)); // ƽ����������λ��
			_program.update("model", model); // ����ģ�;���
			_program.update("textureSampler", 3);
			_program.update("texColor", glm::vec4(1.0, 1.0, 1.0, 0.0));
			glDrawArrays(GL_TRIANGLES, 0, 6); // ����������
		}
	}

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
