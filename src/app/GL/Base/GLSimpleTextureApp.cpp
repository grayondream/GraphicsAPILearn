#include "GLSimpleTextureApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Rect.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include <Utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLSimpleTextureApp::~GLSimpleTextureApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbos.data());
		glDeleteBuffers(1, &_ebo);
	}

	_program.destroy();
}

bool GLSimpleTextureApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);

	createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLSimpleTextureApp::createVertexBuffer() {
	Rect shape{};
	unsigned int vbos[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbos);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

	}
	glBindVertexArray(0);
	_vao = vao;
	_ebo = ebo;
	_vbos = { vbos[0], vbos[1] };
}

void GLSimpleTextureApp::clearColor() {
	return GLApp::clearColor();
}

void GLSimpleTextureApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLSimpleTextureApp::drawScene(const float dt) {
	_texture->texture()->bind(0);
	_program.use();
	glBindVertexArray(_vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	return GLApp::drawScene(dt);
}

void GLSimpleTextureApp::endDrawScene() {
	return GLApp::endDrawScene();
}
