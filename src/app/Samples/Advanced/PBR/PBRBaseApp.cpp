#include "PBRBaseApp.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "base/Log.hpp"
#include "geometry/Sphere.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

PBRBaseApp::~PBRBaseApp() {}

void PBRBaseApp::initShapes() {
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
    _sphereVb = m_sphere.vertexBuffer;
    _sphereUv = m_sphere.uvBuffer;
    _sphereNormal = m_sphere.normalBuffer;
    _sphereIndexCount = m_sphere.indexCount;
}

std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> PBRBaseApp::GetLightPosAndColor() {
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

bool PBRBaseApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!CameraBaseApp::load(rhiRenderer)) return false;
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    initShapes();
    compileShader(m_sphere.layout);
    return true;
}

void PBRBaseApp::compileShader(const rhi::VertexLayout& layout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Base");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "PBR.vs"), "main", false},
                                {rhi::ShaderStage::Fragment, join(shaderDir, "PBR.fs"), "main", false} });
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    m_program = renderer()->createPipeline(layout, shader);
    m_program->setDepthTest(true);
}

std::vector<glm::vec3> PBRBaseApp::GenreateObjPos(int radius, float gap, const glm::vec3& center) {
    std::vector<glm::vec3> positions;
    if (radius < 0) return positions;
    for (int row = -radius; row <= radius; ++row) {
        for (int col = -radius; col <= radius; ++col) {
            float x = center.x + static_cast<float>(col) * gap;
            float y = center.y + static_cast<float>(row) * gap;
            positions.push_back(glm::vec3(x, y, center.z));
        }
    }
    return positions;
}

void PBRBaseApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_sphereVb);
    renderer()->setVertexBuffer(_sphereUv, 1);
    renderer()->setVertexBuffer(_sphereNormal, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    rhi::SetUniform(_ubo, "model", model);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    rhi::SetUniform(_ubo, "normalMatrix", normal);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(_sphereIndexCount, 0, 0);
}

void PBRBaseApp::draw(const float dt) {
    auto pos = _camera.getAttr().pos;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

    renderer()->setPipeline(m_program);
    rhi::SetUniform(_ubo, "ao", m_ao);
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "camPos", pos);
    rhi::SetUniform(_ubo, "roughness", m_roughness);
    rhi::SetUniform(_ubo, "metallic", m_metallic);
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        rhi::SetUniform(_ubo, "albedo", glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f));
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

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
    ImGui::End();
}
