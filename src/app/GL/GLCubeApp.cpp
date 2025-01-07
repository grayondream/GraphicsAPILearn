#include "GLCubeApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollectorPredefined.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Shape/Cube.hpp"
#include "Shape/Rect.hpp"
#include "Shape/Triangle.hpp"
#include "Native/GL/GLTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"

using namespace ErrorHandle;

GLCubeApp::~GLCubeApp() {
	glDeleteVertexArrays(1, &_vao);
	glDeleteBuffers(2, _vbo);
	glDeleteBuffers(1, &_ebo);
}

bool GLCubeApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	const auto vfile = StaticCollector::getGLShaderPath() / "Shape" / "cube.vert";
	const auto ffile = StaticCollector::getGLShaderPath() / "Shape" / "cube.frag";
	auto ret = _program.init(vfile.string(), ffile.string());
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	const auto imgFile = StaticCollector::getImagePath() / "dog.jpg";
	_texture = std::make_shared<GLImageTexture2D>(imgFile.string());
	ExitIfFailed(_texture->load().texture() != GL_INVALID_INDEX, "Failed to load texture from file {}", imgFile.string());
	_texture->multiSurface(6);
	createVertexBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLCubeApp::createVertexBuffer() {
	Cube shape{};
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glGenBuffers(1, &ebo);

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
		glBufferData(GL_ARRAY_BUFFER, _texture->coordSize(), _texture->coord(), GL_STATIC_DRAW);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);
	}
	glBindVertexArray(0);
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
	_ebo = ebo;
}

void GLCubeApp::clearColor() {
	return GLApp::clearColor();
}

void GLCubeApp::beginDrawScene() {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture->texture());
	_program.use();
	return GLApp::beginDrawScene();
}

void GLCubeApp::drawScene() {
	glBindVertexArray(_vao);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	return GLApp::drawScene();
}

void GLCubeApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLCubeApp::updateScene(const float dt) {
	{
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		model = glm::rotate(model, dt, glm::vec3(1.0, 1.0f, 0.0f));
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));
		projection = glm::perspective(glm::radians(45.0f), aspectRatio(), 0.1f, 100.0f);
		_program.update("model", model);
		_program.update("view", view);
		_program.update("projection", projection);
	}
	return GLApp::updateScene(dt);
}