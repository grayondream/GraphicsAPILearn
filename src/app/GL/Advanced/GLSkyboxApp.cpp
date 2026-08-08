
#include "GLSkyboxApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "geometry/Cube.hpp"
#include "geometry/Sphere.hpp"
#include "native/GL/GLImageTexture2D.hpp"
#include "native/GL/GLImageTexture3D.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Image.hpp"
#include "utils/GL/GLUtils.hpp"
#include "utils/GL/GLAppUtils.hpp"
#include "utils/FileUtils.hpp"
using FileUtils::join;
using namespace ErrorHandle;

GLSkyboxApp::~GLSkyboxApp() {
	if (_vao != 0) {
		glDeleteVertexArrays(1, &_vao);
		glDeleteBuffers(3, _vbo);
	}

	glDeleteVertexArrays(1, &_skyVao);
	glDeleteBuffers(1, &_skyVbo);
	_program.destroy();
}

static std::pair<unsigned int, unsigned int> CreateSkyBoxBuffer() {
	Cube shape{};
	unsigned int skyboxVAO, skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
	return { skyboxVAO, skyboxVBO };
}

bool GLSkyboxApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}
	
	_program = GLUtils::CompileShader("Advanced", "SkyBox", "Cube");
	_skyboxProgram = GLUtils::CompileShader("Advanced", "SkyBox", "SkyBox");
	const auto imgPath = StaticCollector::getImagePath();
	const auto imgFile = join(imgPath, "dog.jpg");
	_texture = std::make_shared<GLImageTexture2D>(imgFile);
	_skyBoxTexture = std::make_shared<GLImageTexture3D>(join(imgPath, "Skybox"));
	auto valid = _skyBoxTexture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);

	valid = _texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	createVertexBuffer();
	std::tie(_skyVao, _skyVbo) = CreateSkyBoxBuffer();
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLSkyboxApp::createVertexBuffer() {
	Cube shape{};
	unsigned int vbo[3]{}, vao{}, ebo{};
	glGenVertexArrays(1, &vao);
	glGenBuffers(3, vbo);
	glBindVertexArray(vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, shape.byteSize(), shape.toGL().data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 4));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, shape.normalSize(), shape.normal(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4DBase<float>), nullptr);
		glEnableVertexAttribArray(2); // ����

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.idxByteSize(), shape.idx(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, shape.uvSize(), shape.uv(), GL_STATIC_DRAW);

		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(3);
	}
	glBindVertexArray(0);
	_vao = vao;
	_vbo[0] = vbo[0], _vbo[1] = vbo[1], _vbo[2] = vbo[2];
}

void GLSkyboxApp::beginDrawScene() {
	_texture->texture()->bind(0);
	_program.use();
	return GLApp::beginDrawScene();
}

void GLSkyboxApp::drawCube() {
	glBindVertexArray(_vao);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	_program.update("projection", projection);
	const auto view = _camera.getViewMatrix();
	_program.update("view", view);
	glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	model = glm::translate(model, glm::vec3(0, 0, 0));
	model = glm::rotate(model, glm::radians(0.f), glm::vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(0, 1, 0));
	_program.update("model", model);
	auto attr = _camera.getAttr();
	_program.update("cameraPos", attr.pos);
	_program.update("textureSampler", 0);
	_program.update("skyBoxSampler", 1);
	_program.update("enableReflection", _enableReflect);
	_program.update("enableRefraction", _enableRefraction);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void GLSkyboxApp::drawSkybox() {
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);  // 禁用深度写入
    
    _skyboxProgram.use();
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    _skyboxProgram.update("projection", projection);

    // 使用仅含旋转分量的视图矩阵
    auto view = glm::mat4(glm::mat3(_camera.getViewMatrix()));
    _skyboxProgram.update("view", view);
    
    _skyboxProgram.update("skybox", 1);
    glBindVertexArray(_skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthMask(GL_TRUE);   // 恢复深度写入
    glDepthFunc(GL_LESS);
}

void GLSkyboxApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Reflection", &_enableReflect);
	ImGui::Checkbox("Enable Refraction", &_enableRefraction);
	if (_enableReflect && _enableRefraction) {
		_enableReflect = _enableRefraction = false;
	}

	ImGui::End();
	_texture->texture()->bind(0);
	_skyBoxTexture->texture()->bind(1);
	
	drawCube();
	// 修改渲染顺序：先绘制天空盒，再绘制其他物体
	drawSkybox();
	
	return GLApp::drawScene(dt);
}
