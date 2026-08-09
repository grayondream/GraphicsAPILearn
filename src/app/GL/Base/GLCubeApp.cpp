#include "GLCubeApp.hpp"
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
#include "native/GL/GLCube.hpp"

using FileUtils::join;

using namespace ErrorHandle;

GLCubeApp::~GLCubeApp() {
	cube_->destroy();
	_program.destroy();
}

bool GLCubeApp::initApp() {
	const auto prop = m_window->getProperties();
	glViewport(0, 0, prop.width, prop.height);
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "Cube.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "Cube.frag");
	auto ret = _program.init(vfile, ffile);
	ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	initializeCube();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLCubeApp::initializeCube() {
	cube_ = std::make_shared<GLCube>();
	cube_->init();
}

void GLCubeApp::clearColor() {
	return GLApp::clearColor();
}

void GLCubeApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLCubeApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	glBindVertexArray(cube_->getVao());
	ImGui::Begin("OpenGL");
	static int count{ 1 };
	ImGui::SetNextItemWidth(200);
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

	static float curTime = 0;
	curTime += dt;
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));
		projection = glm::perspective(glm::radians(45.0f), aspectRatio(), 0.1f, 100.0f);
		_program.update("model", model);
		_program.update("view", view);
		_program.update("projection", projection);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		//glDrawElements(GL_TRIANGLES, cube_->idxSize(), GL_UNSIGNED_INT, 0);
	}
	
	glBindVertexArray(0);
}

void GLCubeApp::endDrawScene() {
	return GLApp::endDrawScene();
}