#include "GLMsaaApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Geometry/Cube.hpp"
#include "Geometry/Rect.hpp"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLMsaaApp::~GLMsaaApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
	}

	glDeleteFramebuffers(1, &_screenFrameBuffer);
	glDeleteRenderbuffers(1, &_screenRbo);
	_program.destroy();
}

bool GLMsaaApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", "Cube.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", "Cube.fs");
		auto ret = _program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
	
	{
		const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", "Post.vs");
		const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", "Post.fs");
		auto ret = _postProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	createVertexBuffer();
	createScreenBuffer();
	createFrameBuffer();
	createPostFrameBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLMsaaApp::createVertexBuffer() {
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

void GLMsaaApp::createScreenBuffer() {
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
	_screenVao = vao;
	_screenEbo = ebo;
	_screenVbo[0] = vbos[0];
	_screenVbo[1] = vbos[1];
}

void GLMsaaApp::createFrameBuffer() {
	unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	const auto attr = _attribute.winAttr;
	const auto width = attr.width, height = attr.height;
	unsigned int textureColorBufferMultiSampled;
    glGenTextures(1, &textureColorBufferMultiSampled);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled, 0);

	unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		ExitIfFailed(false, "Failed to create framebuffer");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_screenFrameBuffer = framebuffer;
	_screenRbo = rbo;
}

void GLMsaaApp::createPostFrameBuffer() {
	unsigned int intermediateFBO;
    glGenFramebuffers(1, &intermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

	const auto attr = _attribute.winAttr;
	const auto width = attr.width, height = attr.height;
    // create a color attachment texture
    unsigned int screenTexture;
    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);	// we only need a color buffer

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		ExitIfFailed(false, "Failed to create framebuffer");
	}
        
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_postFrameBuffer = intermediateFBO;
	_postTexture = screenTexture;
}

void GLMsaaApp::clearColor() {
	return GLApp::clearColor();
}

void GLMsaaApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLMsaaApp::drawFrameBufferMssa() {
	glBindFramebuffer(GL_FRAMEBUFFER, _screenFrameBuffer);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	{
		glm::vec3 cubePositions[] = {
			glm::vec3(0.0f,  0.0f,  0.0f),
		};

		glBindVertexArray(_vao);
		const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
		_program.update("projection", projection);
		const auto view = _camera.getViewMatrix();
		_program.update("view", view);
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		model = glm::translate(model, cubePositions[0]);
		model = glm::scale(model, glm::vec3(2.0f));
		model = glm::rotate(model, glm::radians(45.f), glm::vec3(1.0f, 0.f, 0.f));
		model = glm::rotate(model, glm::radians(45.f), glm::vec3(0.0f, 1.f, 0.f));
		_program.update("model", model);

		glDrawArrays(GL_TRIANGLES, 0, 36);


		glBindVertexArray(0);
	}

	const auto attr = _attribute.winAttr;
	const auto width = attr.width, height = attr.height;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _screenFrameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _postFrameBuffer);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

	_postProgram.use();
	glBindVertexArray(_screenVao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _postTexture); // use the now resolved color attachment as the quad's texture
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLMsaaApp::drawGLMssa() {
	glm::vec3 cubePositions[] = {
		glm::vec3(0.0f,  0.0f,  0.0f),
	  };
  
	  glBindVertexArray(_vao);
	  if(_enableMsaa){
		  glEnable(GL_MULTISAMPLE);
	  } else {
		  glDisable(GL_MULTISAMPLE);
	  }
	  
	  const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	  _program.update("projection", projection);
	  const auto view = _camera.getViewMatrix();
	  _program.update("view", view);
	  glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	  model = glm::translate(model, cubePositions[0]);
	  model = glm::scale(model, glm::vec3(2.0f));
	  model = glm::rotate(model, glm::radians(45.f), glm::vec3(1.0f, 0.f, 0.f));
	  model = glm::rotate(model, glm::radians(45.f), glm::vec3(0.0f, 1.f, 0.f));
	  _program.update("model", model);
  
	  glDrawArrays(GL_TRIANGLES, 0, 36);
	  
  
	  glBindVertexArray(0);
}

void GLMsaaApp::drawScene(const float dt) {
	
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	ImGui::Checkbox("Enable MSSA", &_enableMsaa);
	ImGui::Checkbox("Enable FrameBuffer MSSA", &_enableFrameBufferMssa);
	ImGui::End();

	if(_enableFrameBufferMssa){
		drawFrameBufferMssa();
	}else {
		drawGLMssa();
	}
	
	return GLApp::drawScene(dt);
}

void GLMsaaApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLMsaaApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	switch (msg) {
	case WM_KEYDOWN:
		break;
	case WM_KEYUP:
		break;
	case WM_CHAR:
		const char ch = static_cast<char>(wParam);
		switch (wParam) {
		case 'w':
			_camera.processKeyboardEvent(Camera::Movement::Forward, 0.5); break;
		case 's':
			_camera.processKeyboardEvent(Camera::Movement::Backward, 0.5); break;
		case 'd':
			_camera.processKeyboardEvent(Camera::Movement::Right, 0.5); break;
		case 'a':
			_camera.processKeyboardEvent(Camera::Movement::Left, 0.5); break;
		}
		break;
	}

	return GLApp::onKeyBoardEvent(msg, wParam, lParam);
}

void GLMsaaApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLMsaaApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLMsaaApp::onMouseMove(WPARAM btnState, int x, int y) {
	if (!_mouseClicked) {
		return GLApp::onMouseMove(btnState, x, y);
	}

	if (!_clicked) {
		_clicked = true;
		_lastPos = { (float)x, (float)y };
		return GLApp::onMouseMove(btnState, x, y);
	}

	const float offx = x - _lastPos.x;
	const float offy = y - _lastPos.y;
	_camera.processMouseMove(offx, offy);
	_lastPos = { (float)x, (float)y };
	return GLApp::onMouseMove(btnState, x, y);
}

void GLMsaaApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}