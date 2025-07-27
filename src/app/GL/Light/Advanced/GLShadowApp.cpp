#include "GLShadowApp.hpp"
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
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLShadowApp::~GLShadowApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(3, _cubeVbo);
		glDeleteBuffers(1, &_cubeEbo);
	}

	if(_planeVao != 0) {
		glDeleteVertexArrays(1, &_planeVao);
		glDeleteBuffers(3, _planeVbo);
	}
	
	glDeleteVertexArrays(1, &_screenVao);
	glDeleteBuffers(2, _screenVbo);
	glDeleteBuffers(1, &_screenEbo);

	_depthProgram.destroy();
	_shadowProgram.destroy();
}

bool GLShadowApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(0.0f, 3.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90);
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _texture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	createSphereBuffer();
	createPlaneBuffer();
	createShadowDepthBuffer();
	createScreenBuffer();
	compileShader();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLShadowApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "Advanced", "Shadow", "ShadowMapping.vs");
		const auto ffile = join(shaderDir, "Advanced", "Shadow", "ShadowMapping.fs");
		auto ret = _shadowProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Advanced", "Shadow", "ShadowMappingDepth.vs");
		const auto ffile = join(shaderDir, "Advanced", "Shadow", "ShadowMappingDepth.fs");
		auto ret = _depthProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Advanced", "Shadow", "DebugQuand.vs");
		const auto ffile = join(shaderDir, "Advanced", "Shadow", "DebugQuand.fs");
		auto ret = _debugProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

}

void GLShadowApp::createShadowDepthBuffer(){
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create a texture to hold the depth map
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GetShadowMapWidth(), GetShadowMapHeight(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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

void GLShadowApp::createScreenBuffer() {
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

void GLShadowApp::createSphereBuffer() {
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	{
		// �󶨵�һ�� VBO�����ö���λ��
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, cube.byteSize(), cube.toGL().data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, cube.uvSize(), cube.uv(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, cube.normalSize(), cube.normal(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

		// ������������
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, cube.idxByteSize(), cube.idx(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);

	// ��¼ VBO �� EBO
	_cubeVao = vao;
	_cubeVbo[0] = vbo[0], _cubeVbo[1] = vbo[1], _cubeVbo[2] = vbo[2];
	_cubeEbo = ebo;
}

void GLShadowApp::createPlaneBuffer() {
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


void GLShadowApp::clearColor() {
	return GLApp::clearColor();
}

void GLShadowApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLShadowApp::renderPlane(GLProgram &program, const glm::mat4 &model){
	program.use();
	program.update("model", model);
	glBindVertexArray(_planeVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void GLShadowApp::renderCube(GLProgram &program, const glm::mat4 &model, const int type){
	program.use();
	program.update("model", model);
	program.update("type", type);
	glBindVertexArray(_cubeVao);
	glDrawElements(GL_TRIANGLES, cube.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	program.update("type", 0);
}

void GLShadowApp::renderScene(GLProgram &program, const glm::vec3 &lightPos){
	program.use();
	program.update("debug", _enableDebug);
	{
		auto model = glm::mat4(0.5f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		renderPlane(program, model);
	}
	float scale = 0.5f;
	std::vector<glm::mat4> models;
	{
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.f, 2.0f));
		model = glm::scale(model, glm::vec3(scale));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.2f, 0.0f));
		model = glm::scale(model, glm::vec3(scale));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(1.0f, 0.f, 1.0f));
		model = glm::scale(model, glm::vec3(scale * 2));
		models.push_back(model);
	}

	for (auto i = 0; i < models.size(); i++) {
		renderCube(program, models[i]);
	}

	{
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(scale));
		renderCube(program, model, 2);
	}
}

void GLShadowApp::renderScene2FrameBuffer(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos){
	
	_depthProgram.use();
	_depthProgram.update("lightSpaceMatrix", lightSpaceMatrix);
	glViewport(0, 0, GetShadowMapWidth(), GetShadowMapHeight());
	glBindFramebuffer(GL_FRAMEBUFFER, _shadowDepthMapFbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	{
		_texture->texture()->bind();
		renderScene(_depthProgram, lightPos);	
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
}

void GLShadowApp::renderDepthDebug(){
	const float near_plane = 1.0f, far_plane = 7.5f;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	_debugProgram.use();
	_debugProgram.update("near_plane", near_plane);
	_debugProgram.update("far_plane", far_plane);

	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _shadowDepthMap);
	_debugProgram.update("depthMap", 0);

	{
		glBindVertexArray(_screenVao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
}

void GLShadowApp::renderScene2Screen(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos){
	_shadowProgram.use();
	_shadowProgram.update("diffuseTexture", 0);
	_shadowProgram.update("shadowMap", 1);
	auto attr = _camera.getAttr();
	glm::mat4 projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::mat4 view = _camera.getViewMatrix();
	_shadowProgram.update("projection", projection);
	_shadowProgram.update("view", view);
	// set light uniforms
	_shadowProgram.update("viewPos", attr.pos);
	_shadowProgram.update("lightPos", lightPos);
	_shadowProgram.update("lightSpaceMatrix", lightSpaceMatrix);
	glActiveTexture(GL_TEXTURE0);
	auto woodTexture = GLUtils::Ptr2GLTextureId(_texture->texture()->handle());
	glBindTexture(GL_TEXTURE_2D, woodTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _shadowDepthMap);
	renderScene(_shadowProgram, lightPos);
}

void GLShadowApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Debug", &_enableDebug);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;

	const float near_plane = 1.0f, far_plane = 7.5f;
	glm::vec3 lightPos = glm::vec3(-1.0f, 3.0f, 1.0f);
	glm::mat4 lightProjection, lightView;
	glm::mat4 lightSpaceMatrix;
	lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
	lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
	lightSpaceMatrix = lightProjection * lightView;

	renderScene2FrameBuffer(lightSpaceMatrix, lightPos);
	renderScene2Screen(lightSpaceMatrix, lightPos);	
	//renderDepthDebug();
	return GLApp::drawScene(dt);
}

void GLShadowApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLShadowApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLShadowApp::onMouseDown(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		_mouseClicked = true; break;
	}
	
	return GLApp::onMouseDown(msg, btnState, x, y);
}

void GLShadowApp::onMouseUp(const UINT msg, WPARAM btnState, int x, int y) {
	switch (msg) {
	case WM_LBUTTONUP:
		_mouseClicked = false; break;
	}
	
	return GLApp::onMouseUp(msg, btnState, x, y);
}

void GLShadowApp::onMouseMove(WPARAM btnState, int x, int y) {
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

void GLShadowApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}