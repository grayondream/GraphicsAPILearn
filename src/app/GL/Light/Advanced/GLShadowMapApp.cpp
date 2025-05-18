#include "GLShadowMapApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Config/StaticCollector.hpp"
#include "EH/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include "Geometry/Plane.hpp"
#include <Utils/FileUtils.hpp>
#include "Base/Define.hpp"
#include "Geometry/Rect.hpp"

using FileUtils::join;
using namespace ErrorHandle;

GLShadowMapApp::~GLShadowMapApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(2, _vbo);
		glDeleteBuffers(1, &_ebo);
	}

	glDeleteVertexArrays(1, &_screenVao);
	glDeleteBuffers(2, _screenVbo);
	glDeleteBuffers(1, &_screenEbo);

	_depthProgram.destroy();
	_shadowProgram.destroy();
}

bool GLShadowMapApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _texture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	createVertexBuffer();
	createPlaneBuffer();
	createShadowDepthBuffer();
	createScreenBuffer();
	compileShader();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLShadowMapApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "Advanced", "ShadowMap", "ShadowMapping.vs");
		const auto ffile = join(shaderDir, "Advanced", "ShadowMap", "ShadowMapping.fs");
		auto ret = _shadowProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Advanced", "ShadowMap", "Depth.vs");
		const auto ffile = join(shaderDir, "Advanced", "ShadowMap", "Depth.fs");
		auto ret = _depthProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

void GLShadowMapApp::createShadowDepthBuffer(){
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create a texture to hold the depth map
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowMapWidth, kShadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


	// attach depth texture as FBO's depth attachment
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE); // No color output
	glReadBuffer(GL_NONE); // No color output
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGE("ERROR::FRAMEBUFFER:: Framebuffer is not complete!");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	_shadowDepthMapFbo = depthMapFBO;
	_shadowDepthMap = depthMap;
}

void GLShadowMapApp::createScreenBuffer() {
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

void GLShadowMapApp::createVertexBuffer() {
	unsigned int vbo[2]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(2, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		// �󶨵�һ�� VBO�����ö���λ��
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		// ������������
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);

	// ��¼ VBO �� EBO
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1];
	_ebo = ebo;
}

void GLShadowMapApp::createPlaneBuffer() {
	Plane shape{};
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
	}
	glBindVertexArray(0);
	_planeVao = vao;
	_planeVbo[0] = vbo[0], _planeVbo[1] = vbo[1];
	_planeVbo[2] = vbo[2];
}


void GLShadowMapApp::clearColor() {
	return GLApp::clearColor();
}

void GLShadowMapApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLShadowMapApp::renderScene2FrameBuffer(){
	const float near_plane = 1.0f, far_plane = 7.5f;
	glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
	glm::mat4 lightProjection, lightView;
	glm::mat4 lightSpaceMatrix;        
	lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
	lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
	lightSpaceMatrix = lightProjection * lightView;
	_shadowProgram.use();
	_shadowProgram.update("lightSpaceMatrix", lightSpaceMatrix);
	glViewport(0, 0, kShadowMapWidth, kShadowMapHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, _shadowDepthMapFbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	{
		_shadowProgram.update("model", glm::mat4(1.0f));
		glBindVertexArray(_planeVao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	{
		auto model = glm::mat4(1.0f);
    	model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0));
		_shadowProgram.update("model", model);
		glBindVertexArray(_vao);
		glDrawElements(GL_TRIANGLES, shape.idxSize(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);	
}

void GLShadowMapApp::reanderFraemBuffer(){
	const float near_plane = 1.0f, far_plane = 7.5f;
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	_depthProgram.update("near_plane", near_plane);
	_depthProgram.update("far_plane", far_plane);

	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _shadowDepthMap);
	_depthProgram.update("textureSampler", 0);

	{
		glBindVertexArray(_screenVao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}


void GLShadowMapApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::End();

	static float curTime = 0;
	curTime += dt;

	renderScene2FrameBuffer();
	reanderFraemBuffer();
	return GLApp::drawScene(dt);
}

void GLShadowMapApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLShadowMapApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLShadowMapApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLShadowMapApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLShadowMapApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLShadowMapApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}