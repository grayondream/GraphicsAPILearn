#include "GLAdvancedGLSLApp.hpp"
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
#include "utils/FileUtils.hpp"
using FileUtils::join;

using namespace ErrorHandle;

GLAdvancedGLSLApp::~GLAdvancedGLSLApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
	}

	_program.destroy();
}

bool GLAdvancedGLSLApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "GLSL", "Cube.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "GLSL", "Cube.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	createVertexBuffer();
	return true;
}

void GLAdvancedGLSLApp::createVertexBuffer() {
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
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
}

void GLAdvancedGLSLApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLAdvancedGLSLApp::drawScene(const float dt) {
	glBindVertexArray(_vao);
	ImGui::Begin("OpenGL");
	static int count{ 1 };
	ImGui::Checkbox("Enable Point Size", &_enablePointSize);
    ImGui::Checkbox("Enable Frag Coord", &_enableFragCoord);
    ImGui::Checkbox("Enable Vertex Id", &_enableVertexId);
    ImGui::Checkbox("Enable Front Face Culling", &_enableFrontFaceCulling);
	ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();
	glm::vec3 cubePositions[] = {
	  glm::vec3(0.0f,  0.0f,  0.0f),
	  glm::vec3(2.0f,  5.0f, -15.0f),
	  glm::vec3(-1.5f, -2.2f, -2.5f),
	  glm::vec3(-3.8f, -2.0f, -12.3f),
	  glm::vec3(2.4f, -0.4f, -3.5f),
	  glm::vec3(-1.7f,  3.0f, -7.5f),
	  glm::vec3(1.3f, -2.0f, -2.5f),
	  glm::vec3(1.5f,  2.0f, -2.5f),
	  glm::vec3(1.5f,  0.2f, -1.5f),
	  glm::vec3(-1.3f,  1.0f, -1.5f)
	};
    if(_enablePointSize){
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        //glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
        glEnable(GL_PROGRAM_POINT_SIZE);
    }else{
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_PROGRAM_POINT_SIZE);
        //glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    }

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		_program.update("model", model);
        _program.update("enablePointSize", _enablePointSize);
        _program.update("enableFragCoord", _enableFragCoord);
        _program.update("enableVertexId", _enableVertexId);
        _program.update("enableFrontFaceCulling", _enableFrontFaceCulling);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
