#include "GLRectApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Shape/Rect.hpp"

GLRectApp::~GLRectApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(1, &_vbo);
		glDeleteBuffers(1, &_ebo);
	}
}

bool GLRectApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}

	const auto vfile = StaticCollector::getGLShaderPath() / "Shape" / "rect.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Shape" / "rect.frag";
	auto ret = _program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	std::tie(_vao, _vbo, _ebo) = createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

std::tuple<unsigned int, unsigned int, unsigned int> GLRectApp::createVertexBuffer() {
	Rect shape{};
	unsigned int vbo{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}
	glBindVertexArray(0);
	return {vao, vbo, ebo};
}

void GLRectApp::clearColor() {
	return GLApp::clearColor();
}

void GLRectApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLRectApp::drawScene() {
	_program.use();
	glBindVertexArray(_vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	return GLApp::drawScene();
}

void GLRectApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLRectApp::updateScene(const float dt) {
	return GLApp::updateScene(dt);
}