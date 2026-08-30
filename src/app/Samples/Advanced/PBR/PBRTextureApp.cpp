#include "PBRTextureApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
#include "app/Samples/RhiImage.hpp"
using FileUtils::join;
using namespace ErrorHandle;

PBRTextureApp::~PBRTextureApp() {}

void PBRTextureApp::loadTexture() {
    auto resDir = join(StaticCollector::getImagePath(), "rusted_iron");
    auto load = [&](const std::string& name) {
        auto tex = RhiImage::Load2D(renderer().get(), join(resDir, name));
        ExitIfFailed(tex != nullptr, "Failed to load texture from file {}", name);
        return tex;
    };
    m_albedoMap = load("albedo.png");
    m_roughnessMap = load("roughness.png");
    m_metallicMap = load("metallic.png");
    m_aoMap = load("ao.png");
    m_normalMap = load("normal.png");
}

bool PBRTextureApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!PBRBaseApp::load(rhiRenderer)) return false;
    loadTexture();
    compileShader();
    return true;
}

void PBRTextureApp::compileShader() {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Texture");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "PBR.vs"), "main", false},
                                {rhi::ShaderStage::Fragment, join(shaderDir, "PBR.fs"), "main", false} });
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    m_program = renderer()->createPipeline(m_sphere.layout, shader);
    m_program->setDepthTest(true);
}

void PBRTextureApp::draw(const float dt) {
    auto pos = _camera.getAttr().pos;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

    renderer()->setPipeline(m_program);
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "camPos", pos);

    renderer()->bindTexture(m_albedoMap, 1);
    renderer()->bindTexture(m_roughnessMap, 2);
    renderer()->bindTexture(m_metallicMap, 3);
    renderer()->bindTexture(m_aoMap, 4);
    renderer()->bindTexture(m_normalMap, 5);

    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(m_program, objectPos);
    }

    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        rhi::SetUniform(_ubo, "lightPositions", i, lightPositions[i]);
        rhi::SetUniform(_ubo, "lightColors", i, lightColors[i]);
        renderSphere(m_program, lightModel);
    }

    ImGui::Begin(rhi::backendDisplayName());
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}
