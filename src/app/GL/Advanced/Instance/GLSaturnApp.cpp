#include "GLSaturnApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "geometry/Cube.hpp"
#include "native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
#include <model/Model.hpp>
using FileUtils::join;

using namespace ErrorHandle;

GLSaturnApp::~GLSaturnApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_saturnProgram.destroy();
}

std::vector<glm::mat4> GenerateRocksPosition(int amount, const glm::mat4& pos) {
	std::vector<glm::mat4> modelMatrices;
	modelMatrices.resize(amount);
	srand(static_cast<unsigned int>(0)); // initialize random seed
	float radius = 20.0;
	float offset = 10.0f;
	for (unsigned int i = 0; i < amount; i++)
	{
		glm::mat4 model = pos;
		// 1. translation: displace along circle with 'radius' in range [-offset, offset]
		float angle = (float)i / (float)amount * 360.0f;
		float displacement = (rand() % (int)(20 * offset * 10)) / 10.0f - offset;
		float x = sin(angle) * radius + displacement;
		displacement = (rand() % (int)(2 * offset * 10)) / 10.0f - offset;
		float y = displacement * 0.4f; // keep height of asteroid field smaller compared to width of x and z
		displacement = (rand() % (int)(2 * offset * 10)) / 10.0f - offset;
		float z = cos(angle) * radius + displacement;
		model = glm::translate(model, glm::vec3(x, y, z));

		// 2. scale: Scale between 0.05 and 0.25f
		float scale = static_cast<float>((rand() % 20) / 100.0 + 0.05);
		model = glm::scale(model, glm::vec3(scale));

		// 3. rotation: add random rotation around a (semi)randomly picked rotation axis vector
		float rotAngle = static_cast<float>((rand() % 360));
		model = glm::rotate(model, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

		// 4. now add to list of matrices
		modelMatrices[i] = model;
	}

	return modelMatrices;
}

unsigned int GenerateRockPoisitonBuffer(int count = 100, const glm::mat4& pos = glm::mat4(1.0)) {
	const auto poses = GenerateRocksPosition(count, pos);
	unsigned int buffer{};
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, poses.size() * sizeof(poses[0]), poses.data(), GL_STATIC_DRAW);
	return buffer;
}

bool GLSaturnApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	_saturnPos = glm::vec3(0, 0, -3);
	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.fs");
		GLProgram program{};
		auto ret = program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
		_saturnProgram = program;
	}
	
	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.fs");
		GLProgram program{};
		auto ret = program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
		_rockProgram = program;
	}

	loadModel();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//createVertexBuffer();
	return true;
}

void GLSaturnApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	{
		const auto modelFile = join(modelPath, "planet", "planet.obj");
		_saturn = std::make_shared<Model>(modelFile);
	}

	{
		const auto modelFile = join(modelPath, "rock", "rock.obj");
		_rock = std::make_shared<Model>(modelFile);
	}

	{
		_count = 30000;
		const auto buffer = GenerateRockPoisitonBuffer(_count);
		for (unsigned int i = 0; i < _rock->meshes.size(); i++)
		{
			unsigned int VAO = _rock->meshes[i]._vao;
			glBindVertexArray(VAO);
			// set attribute pointers for matrix (4 times vec4)
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
			glEnableVertexAttribArray(5);
			glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
			glEnableVertexAttribArray(6);
			glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

			glVertexAttribDivisor(3, 1);
			glVertexAttribDivisor(4, 1);
			glVertexAttribDivisor(5, 1);
			glVertexAttribDivisor(6, 1);

			glBindVertexArray(0);
		}
	}
}

void GLSaturnApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	
	float radius = 5.0f; // 旋转半径
	auto model = glm::translate(glm::mat4(1.0), _saturnPos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	const float scale = 0.3;
	model = glm::scale(model, glm::vec3(scale, scale, scale));
	//draw light source
	{
		_saturnProgram.use();
		_saturnProgram.update("projection", projection);
		_saturnProgram.update("view", view);
		model = glm::rotate(model, glm::radians(curTime * 5), glm::vec3(1.0, 1.0, 0.0));
		_saturnProgram.update("model", model);
		_saturn->draw(_saturnProgram);
	}

	{
		_rockProgram.use();
		_rockProgram.update("projection", projection);
		_rockProgram.update("view", view);
		model = glm::translate(model, glm::vec3(0.0f, 0.f, 0.0f)); // translate it down so it's at the center of the scene
		_rockProgram.update("model", model);
		_rockProgram.update("time", curTime);
		_rockProgram.update("radiusPos", _saturnPos);
		_rock->draw(_rockProgram, _count);
	}

	return GLApp::drawScene(dt);
}
