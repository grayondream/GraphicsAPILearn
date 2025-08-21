#include "GLPBRBaseApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include "Native/GL/GLProgram.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "Native/GL/GLImageTexture2D.hpp"
#include "Native/GL/GLCube.hpp"
#include "Native/GL/GLPlane.hpp"
#include "Base/Log.hpp"
#include "imgui.h"
#include "Utils/FileUtils.hpp"
#include "Geometry/Rect.hpp"
#include "Utils/GL/GLUtils.hpp"
#include "Base/Constexpr.hpp"


using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLPBRBaseApp::~GLPBRBaseApp() {
    
}

void GLPBRBaseApp::initShapes() {
	m_sphere.init();	
}

bool GLPBRBaseApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	compileShader();
	initShapes();	
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgname){
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
}

void GLPBRBaseApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Base");
	{
		const auto vfile = join(shaderDir, "PBR.vs");
		const auto ffile = join(shaderDir, "PBR.fs");
		auto ret = m_program.init(vfile, ffile);
		ErrorHandle::ExitIfFailed(ret, "Create OpenGL program failed!");
	}
}

static auto GetLightPosAndColor(){
	std::vector<glm::vec3> lightPositions = {
        glm::vec3(-1.0f,  1.0f, 1.0f),
        glm::vec3( 1.0f,  1.0f, 1.0f),
        glm::vec3(-1.0f, -1.0f, 1.0f),
        glm::vec3( 1.0f, -1.0f, 1.0f),
    };
	std::vector<glm::vec3> lightColors = {
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f)
    };

    return std::make_pair(lightPositions, lightColors);
}

void GLPBRBaseApp::renderSphere(GLProgram &program, const glm::mat4 &model) {
	m_program.use();
	program.update("model", model);
	const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
	program.update("normalMatrix", normal);
	glBindVertexArray(m_sphere.getVao());
	glDrawElements(GL_TRIANGLES, m_sphere.idxSize(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void GLPBRBaseApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	m_program.use();
	m_program.update("albedo", glm::vec3(0.5f, 0.5f, 0.5f));
    m_program.update("ao", 1.0f);
	m_program.update("projection", projection);
	m_program.update("view", view);
	m_program.update("camPos", pos);
	m_program.update("roughness", m_roughness);
    m_program.update("metallic", 0.0f);
	auto objectPos = glm::mat4(1.0f);
    objectPos = glm::translate(objectPos, glm::vec3(0.0f, 0.0f, -2.0f));
	renderSphere(m_program, objectPos);

	m_program.use();
	const auto lightPosAndColor = GetLightPosAndColor();
	for (int i = 0; i < lightPosAndColor.first.size(); ++i) {
		const auto lightPos = lightPosAndColor.first[i];
		const auto lightColor = lightPosAndColor.second[i];
		m_program.update("lightPositions[" + std::to_string(i) + "]", lightPos);
        m_program.update("lightColors[" + std::to_string(i) + "]", lightColor);

		auto lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, lightPos);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f));
		m_program.update("model", lightModel);
		renderSphere(m_program, lightModel);
	}

	ImGui::Begin("OpenGL");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
	ImGui::End();
}