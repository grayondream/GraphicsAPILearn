#include "GLTriangleApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Triangle.hpp"
#include <Utils/FileUtils.hpp>
using FileUtils::join;

GLTriangleApp::~GLTriangleApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(1, &_vbo);
	}

	_program.destroy();
}

bool GLTriangleApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	std::tie(_vao, _vbo) = createVertexBuffer();
	return true;
}

std::pair<unsigned int, unsigned int> GLTriangleApp::createVertexBuffer() {
	Triangle oneTriangle{};
	unsigned int vbo{}, vao{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, oneTriangle.byteSize(), oneTriangle.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);
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

void GLTriangleApp::drawScene(const float dt) {
	_program.use();
	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	return GLApp::drawScene(dt);
}

void GLTriangleApp::endDrawScene() {
	return GLApp::endDrawScene();
}
