#include "GLPBRBaseApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include "native/GL/GLProgram.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "glad/glad.h"
#include "native/GL/GLImageTexture2D.hpp"
#include "native/GL/GLCube.hpp"
#include "native/GL/GLPlane.hpp"
#include "base/Log.hpp"
#include "imgui.h"
#include "utils/FileUtils.hpp"
#include "geometry/Rect.hpp"
#include "utils/GL/GLUtils.hpp"
#include "base/Constexpr.hpp"


using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLPBRBaseApp::~GLPBRBaseApp() {
    
}

void GLPBRBaseApp::initShapes() {
	m_sphere.init();	
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgname){
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = std::make_shared<GLImageTexture2D>(imgFile);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
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
        glm::vec3(-10.0f,  10.0f, 10.0f),
        glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    std::vector<glm::vec3> lightColors = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f)
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

static std::vector<glm::vec3> GenreateObjPos(int radius = 5, float gap = 0.5f, const glm::vec3 &center = glm::vec3(0.0f)) {
    std::vector<glm::vec3> positions;
    if (radius < 0) {
        return positions;
    }

    for (int row = -radius; row <= radius; ++row) {
        for (int col = -radius; col <= radius; ++col) {
            float x = center.x + static_cast<float>(col) * gap;
            float y = center.y + static_cast<float>(row) * gap;
            positions.push_back(glm::vec3(x, y, center.z));
        }
    }
    
    return positions;
}

void GLPBRBaseApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

	m_program.use();
	m_program.update("texture", 0);
    m_program.update("ao", m_ao);
	m_program.update("projection", projection);
	m_program.update("view", view);
	m_program.update("camPos", pos);
	m_program.update("roughness", m_roughness);
    m_program.update("metallic", m_metallic);
	const int cnt = objPos.size();
	for(int i = 0;i < cnt;i ++){
		const auto pos = objPos[i];
		auto objectPos = glm::mat4(1.0f);
		m_program.update("albedo", glm::vec3(i * 1.0f/ cnt, 0.0f, 0.0f));
		objectPos = glm::translate(objectPos, pos);
		objectPos = glm::scale(objectPos, glm::vec3(0.4f));
		renderSphere(m_program, objectPos);
	}

	const auto lightPosAndColor = GetLightPosAndColor();
	for (int i = 0; i < lightPosAndColor.first.size(); ++i) {
		const auto lightPos = lightPosAndColor.first[i];
		auto lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, lightPos);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f));
		m_program.update("lightPositions[" + std::to_string(i) + "]", lightPosAndColor.first[i]);
		m_program.update("lightColors[" + std::to_string(i) + "]", lightPosAndColor.second[i]);
		renderSphere(m_program, lightModel);
	}

	ImGui::Begin("OpenGL");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
	ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
	ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
	ImGui::End();
}