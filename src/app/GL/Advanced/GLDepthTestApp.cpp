#include "GLDepthTestApp.hpp"
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

GLDepthTestApp::~GLDepthTestApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(2, _cubeVbo);
	}

	_program.destroy();
}

bool GLDepthTestApp::initApp() {
	if (!GLApp::initApp()) {
		return false;
	}

	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "DepthTest", "Basic.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "DepthTest", "Basic.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
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
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LESS);
	return true;
}

void GLDepthTestApp::createCubeBuffer() {
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

void GLDepthTestApp::createPlaneBuffer() {
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

void GLDepthTestApp::beginDrawScene() {
	_cubeTexture->texture()->bind(0);
	_planeTexture->texture()->bind(1);
	_program.use();
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

void GLDepthTestApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	//ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();
	
	std::vector<glm::vec3> cubePositions = initializeCubePositions();
	int count = cubePositions.size(); // ����������

	// ����ͶӰ����
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);

	// ��ȡ��ͼ����
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);

	static float curTime = 0; // ���ֵ�ǰʱ��
	curTime += dt; // ���µ�ǰʱ��
	glBindVertexArray(_cubeVao);
	_program.update("textureSampler", 0);
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
		model = glm::translate(model, cubePositions[i]); // ƽ����������λ��

		// ʹ�õ�ǰʱ�������������������ת�Ƕ�
		float angle = 0; // ÿ���������Բ�ͬ���ٶ���ת
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f)); // ��ת

		_program.update("model", model); // ����ģ�;���
		glDrawArrays(GL_TRIANGLES, 0, 36); // ����������
	}

	glBindVertexArray(_planeVao);
	glm::mat4 model = glm::mat4(1.0f); // ��ʼ������Ϊ��λ����
	model = glm::translate(model, glm::vec3(-1.0,-4.50, -10)); // ƽ����������λ��
	_program.update("model", model); // ����ģ�;���
	_program.update("textureSampler", 1);
	glDrawArrays(GL_TRIANGLES, 0, 6); // ����������

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
