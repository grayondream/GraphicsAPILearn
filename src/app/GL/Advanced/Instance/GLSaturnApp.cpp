#include "GLSaturnApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "geometry/Cube.hpp"
#include "native/GL/GLImageTexture2D.hpp"
#include "rhi/core/IShader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
#include <model/Model.hpp>
#include <cstddef>
using FileUtils::join;

using namespace ErrorHandle;

GLSaturnApp::~GLSaturnApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	_rockProgram.destroy();
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
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Rock.fs");
		GLProgram program{};
		auto ret = program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
		_rockProgram = program;
	}

	loadModel();
	initSaturnPipeline();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//createVertexBuffer();
	return true;
}

void GLSaturnApp::initSaturnPipeline() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Instance", "Saturn.fs");
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ErrorHandle::ExitIfFailed(ok, "Create Saturn RHI shader failed: {}", shader->getLog());
	_saturnPipeline = renderer()->createPipeline(_saturn->vertexLayout(), shader);
	_saturnPipeline->setDepthTest(true);
}

void GLSaturnApp::loadModel() {
	const auto modelPath = StaticCollector::getModelPath();
	{
		const auto modelFile = join(modelPath, "planet", "planet.obj");
		_saturn = std::make_shared<Model>(renderer().get(), modelFile);
	}

	{
		const auto modelFile = join(modelPath, "rock", "rock.obj");
		_rock = std::make_shared<Model>(renderer().get(), modelFile);
	}

	{
		_count = 30000;
		const auto instanceBuffer = GenerateRockPoisitonBuffer(_count);
		_rockVAOs.resize(_rock->meshes.size());
		_rockIndexCounts.resize(_rock->meshes.size());
		for (size_t i = 0; i < _rock->meshes.size(); i++)
		{
			const auto& mesh = _rock->meshes[i];
			GLuint vao, vbo, ebo;
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glGenBuffers(1, &ebo);
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(MeshVertex), mesh.vertices.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);
			// set attribute pointers for vertex data (positions / normals / texCoords)
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, Normal));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, TexCoords));
			// set attribute pointers for matrix (4 times vec4) with instance divisor
			glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
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
			_rockVAOs[i] = vao;
			_rockIndexCounts[i] = static_cast<unsigned int>(mesh.indices.size());
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
		renderer()->setPipeline(_saturnPipeline);
		_saturnPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_saturnPipeline->setUniform("view", glm::value_ptr(view), 1);
		model = glm::rotate(model, glm::radians(curTime * 5), glm::vec3(1.0, 1.0, 0.0));
		_saturnPipeline->setUniform("model", glm::value_ptr(model), 1);
		_saturn->draw(renderer().get(), _saturnPipeline.get());
	}

	{
		_rockProgram.use();
		_rockProgram.update("projection", projection);
		_rockProgram.update("view", view);
		model = glm::translate(model, glm::vec3(0.0f, 0.f, 0.0f)); // translate it down so it's at the center of the scene
		_rockProgram.update("model", model);
		_rockProgram.update("time", curTime);
		_rockProgram.update("radiusPos", _saturnPos);
		for (size_t i = 0; i < _rockVAOs.size(); i++) {
			glBindVertexArray(_rockVAOs[i]);
			glDrawElementsInstanced(GL_TRIANGLES, _rockIndexCounts[i], GL_UNSIGNED_INT, 0, _count);
		}
		glBindVertexArray(0);
	}

	return GLApp::drawScene(dt);
}
