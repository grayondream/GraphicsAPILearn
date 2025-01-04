#include "GLTriangleApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"

GLTriangleApp::~GLTriangleApp() {
	glDeleteVertexArrays(1, &_vao);
	glDeleteBuffers(1, &_vbo);
}

bool GLTriangleApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	const auto vfile = StaticCollector::getGLShaderPath() / "Shape" / "triangle.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Shape" / "triangle.frag";
	auto ret = _program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	std::tie(_vao, _vbo) = createVertexBuffer();
	return true;
}

std::pair<unsigned int, unsigned int> GLTriangleApp::createVertexBuffer() {
	float vertices[] = {
		-0.5f, -0.5f, 0.0f, // left  
		 0.5f, -0.5f, 0.0f, // right 
		 0.0f,  0.5f, 0.0f  // top   
	};

	unsigned int vbo{}, vao{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);
	}
	glBindVertexArray(0);
	return {vao, vbo};
}

void GLTriangleApp::clearColor() {
	return GLApp::clearColor();
}

void GLTriangleApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLTriangleApp::drawScene() {
	_program.use();
	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GLTriangleApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLTriangleApp::updateScene(const float dt) {
}