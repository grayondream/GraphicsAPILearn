#include "GLPointLightShadowApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Base/Log.hpp"
#include "imgui.h"
#include <Utils/FileUtils.hpp>
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLPointLightShadowApp::~GLPointLightShadowApp() {
	if (_cubeVao != 0) {
		glDeleteVertexArrays(1, &_cubeVao);
		glDeleteBuffers(3, _cubeVbo);
		glDeleteBuffers(1, &_cubeEbo);
	}

	_depthProgram.destroy();
	_shadowProgram.destroy();
}

bool GLPointLightShadowApp::init(const HINSTANCE inst, const WindowDesc& param) {
	if (!GLApp::init(inst, param)) {
		return false;
	}
	
	_camera = Camera(glm::vec3(-1.f, 0.0f, 1.0f));
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = std::make_shared<GLImageTexture2D>(imgFile);
		const auto valid = _texture->load().texture()->valid();
		ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	}

	glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
	createCubeBuffer();
	createShadowDepthBuffer();
	compileShader();
	
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLPointLightShadowApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		const auto vfile = join(shaderDir, "Advanced", "PointLightShadow", "ShadowMapping.vs");
		const auto ffile = join(shaderDir, "Advanced", "PointLightShadow", "ShadowMapping.fs");
		auto ret = _shadowProgram.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}

	{
		const auto vfile = join(shaderDir, "Advanced", "PointLightShadow", "ShadowMappingDepth.vs");
		const auto ffile = join(shaderDir, "Advanced", "PointLightShadow", "ShadowMappingDepth.fs");
		const auto gfile = join(shaderDir, "Advanced", "PointLightShadow", "ShadowMappingDepth.gs");
		auto ret = _depthProgram.init(vfile, ffile, gfile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

void GLPointLightShadowApp::createShadowDepthBuffer(){
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create a texture to hold the depth map
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthMap);
	for (unsigned int i = 0; i < 6; ++i){
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, GetShadowMapWidth(), GetShadowMapHeight(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ErrorHandle::ExitIfFailed(false, "Shadow FBO is incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	_shadowDepthMapFbo = depthMapFBO;
	_shadowDepthMap = depthMap;
}

void GLPointLightShadowApp::createCubeBuffer() {
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


void GLPointLightShadowApp::clearColor() {
	return GLApp::clearColor();
}

void GLPointLightShadowApp::beginDrawScene() {
	return GLApp::beginDrawScene();
}

void GLPointLightShadowApp::renderCube(GLProgram &program, const glm::mat4 &model, const int type){
	program.use();
	program.update("model", model);
	program.update("type", type);
	glBindVertexArray(_cubeVao);
	glDrawElements(GL_TRIANGLES, cube.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	program.update("type", 0);
}

void GLPointLightShadowApp::renderScene(GLProgram &program, const glm::vec3 &lightPos){
	program.use();
	float scale = 0.25f;
	std::vector<glm::mat4> models;
	{
		glm::mat4 model = glm::mat4(1.0f);
    	model = glm::scale(model, glm::vec3(10.0f));
		glDisable(GL_CULL_FACE);
		program.update("reverse_normals", 1);
		renderCube(program, model);
		program.update("reverse_normals", 0);
		glEnable(GL_CULL_FACE);
	}

	{
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0));
		model = glm::scale(model, glm::vec3(0.5f));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
    	model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0));
    	model = glm::scale(model, glm::vec3(0.75f));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
    	model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0));
    	model = glm::scale(model, glm::vec3(0.5f));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
    	model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5));
    	model = glm::scale(model, glm::vec3(0.5f));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
    	model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0));
    	model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    	model = glm::scale(model, glm::vec3(0.75f));
		models.push_back(model);
	}

	{
		auto model = glm::mat4(1.0f);
		model = glm::scale(model, glm::vec3(0.1f));
		model = glm::translate(model, lightPos);
		program.update("light", 1);
		renderCube(program, model);
		program.update("light", 0);
	}

	for (auto i = 0; i < models.size(); i++) {
		renderCube(program, models[i]);
	}
}

std::vector<glm::mat4> CreateTransformVector(const glm::vec3& lightPos, float aspectRatio, float far_plane, float near_plane){
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspectRatio, near_plane, far_plane);
	std::vector<glm::mat4> shadowTransforms;
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	return shadowTransforms;
}

void GLPointLightShadowApp::renderScene2FrameBuffer(GLProgram& program, const glm::vec3 &lightPos){
	auto shadowTransforms = CreateTransformVector(lightPos, GetShadowMapWidth() * 1.0 / GetShadowMapHeight(), _far, _near);
	program.use();
	glViewport(0, 0, GetShadowMapWidth(), GetShadowMapHeight());
	glBindFramebuffer(GL_FRAMEBUFFER, _shadowDepthMapFbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	{
		for (unsigned int i = 0; i < 6; ++i){
			program.update("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
		}
                
		program.update("far_plane", _far);
		program.update("lightPos", lightPos);
		renderScene(program, lightPos);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, _attribute.winAttr.width, _attribute.winAttr.height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLPointLightShadowApp::renderScene2Screen(GLProgram& program, const glm::vec3 &lightPos){
	program.use();
	program.update("diffuseTexture", 0);
	program.update("depthMap", 1);
	auto attr = _camera.getAttr();
	glm::mat4 projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::mat4 view = _camera.getViewMatrix();
	program.update("projection", projection);
	program.update("view", view);
	program.update("lightPos", lightPos);
	program.update("viewPos", attr.pos);
	program.update("shadows", _enableShadow); // enable/disable shadows by pressing 'SPACE'
	program.update("far_plane", _far);

	auto woodTexture = GLUtils::Ptr2GLTextureId(_texture->texture()->handle());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, woodTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _shadowDepthMap);
	renderScene(_shadowProgram, lightPos);
}

void GLPointLightShadowApp::drawScene(const float dt) {
	static float curTime = 0;
	curTime += dt;
	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
	lightPos.z = static_cast<float>(sin(curTime) * 10.0);
	GLApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable PCF", &_enableSimplePCF);
	ImGui::Checkbox("Enable Shadow", &_enableShadow);
	ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Light Pos: (%.2f, %.2f, %.2f)", lightPos.x, lightPos.y, lightPos.z);
	ImGui::End();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	
	renderScene2FrameBuffer(_depthProgram,lightPos);
	renderScene2Screen(_shadowProgram,lightPos);
	return GLApp::drawScene(dt);
}

void GLPointLightShadowApp::endDrawScene() {
	return GLApp::endDrawScene();
}

void GLPointLightShadowApp::onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
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

void GLPointLightShadowApp::onMouseMove(WPARAM btnState, int x, int y) {
	// 仅在鼠标被点击时处理移动事件
	if (!_mouseClicked) {
		return GLApp::onMouseMove(btnState, x, y);
	}

	// 计算偏移量
	const float offx = x - _lastx;
	const float offy = y - _lasty;

	// 更新摄像机
	_camera.processMouseMove(offx, offy);
	_lastx = x;
	_lasty = y;
	return GLApp::onMouseMove(btnState, x, y);
}

void GLPointLightShadowApp::onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	_camera.processMouseScrool(zDelta);
	return GLApp::onMouseScroll(msg, wParam, lParam);
}