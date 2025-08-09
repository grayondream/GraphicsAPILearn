#include "GLSimpleGemoteryApp.hpp"
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

GLSimpleGemoteryApp::~GLSimpleGemoteryApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(1, &_vbo);
	}

	_program.destroy();
}

bool GLSimpleGemoteryApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.fs");
	const auto gfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.gs");

	GLProgram program{};
	auto ret = program.init(vfile, ffile, gfile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	_program = program;
	
	createVertexBuffer();
	return true;
}

void GLSimpleGemoteryApp::createVertexBuffer() {
	float points[] = {
        -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, // top-left
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, // top-right
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // bottom-right
        -0.5f, -0.5f, 1.0f, 1.0f, 0.0f  // bottom-left
    };
    unsigned int vbo, vao;
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), &points, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
	
	_vao = vao;
	_vbo = vbo;
}

void GLSimpleGemoteryApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::End();

	_program.use();
	glBindVertexArray(_vao);
	glDrawArrays(GL_POINTS, 0, 4);

	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}
