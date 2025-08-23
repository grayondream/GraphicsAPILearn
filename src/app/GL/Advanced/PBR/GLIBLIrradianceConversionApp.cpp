#include "GLIBLIrradianceConversionApp.hpp"
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

GLIBLIrradianceConversionApp::~GLIBLIrradianceConversionApp() {
    
}

void GLIBLIrradianceConversionApp::initShapes() {
	m_sphere.init();	
}

static std::shared_ptr<GLImageTexture2D> CreateTexture(const std::string &imgFile, bool isHdr = false){
	TextureOption option{};
	option.IsHdr = isHdr;
	auto texture = std::make_shared<GLImageTexture2D>(imgFile, option);
	const auto valid = texture->load().texture()->valid();
	ExitIfFailed(valid, "Failed to load texture from file {}", imgFile);
	return texture;
}

bool GLIBLIrradianceConversionApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	compileShader();
	initShapes();	
	loadTexture();
	glEnable(GL_DEPTH_TEST);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	return true;
}

void GLIBLIrradianceConversionApp::compileShader(){
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Texture");

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

void GLIBLIrradianceConversionApp::renderSphere(GLProgram &program, const glm::mat4 &model) {
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

void GLIBLIrradianceConversionApp::loadTexture(){
	auto resDir = StaticCollector::getImagePath();
	auto rustDir = join(resDir, "rusted_iron");

    m_albedoMap = CreateTexture(join(rustDir, "albedo.png"));
    m_roughnessMap = CreateTexture(join(rustDir, "roughness.png"));
    m_metallicMap = CreateTexture(join(rustDir, "metallic.png"));
    m_aoMap = CreateTexture(join(rustDir, "ao.png"));
	m_normalMap = CreateTexture(join(rustDir, "normal.png"));
    m_hdrEnvTexture = CreateTexture(join(resDir, "newport_loft.hdr"), true);
}

void GLIBLIrradianceConversionApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

	m_program.use();
	m_program.update("texture", 0);
    
	m_program.update("projection", projection);
	m_program.update("view", view);
	m_program.update("camPos", pos);
	m_metallicMap->texture()->bind(3);
	m_normalMap->texture()->bind(5);
	m_aoMap->texture()->bind(4);
	m_albedoMap->texture()->bind(1);
	m_roughnessMap->texture()->bind(2);

	m_program.update("albedoMap", 1);
	m_program.update("roughnessMap", 2);
	m_program.update("metallicMap", 3);
	m_program.update("aoMap", 4);
	m_program.update("normalMap", 5);
	
	const int cnt = objPos.size();
	for(int i = 0;i < cnt;i ++){
		const auto pos = objPos[i];
		auto objectPos = glm::mat4(1.0f);
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
	ImGui::End();
}