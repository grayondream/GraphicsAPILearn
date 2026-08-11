#include "GLPBRBaseApp.hpp"
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

GLPBRBaseApp::~GLPBRBaseApp() {}

void GLPBRBaseApp::initShapes() {
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
    _sphereVb = m_sphere.vertexBuffer;
    _sphereUv = m_sphere.uvBuffer;
    _sphereNormal = m_sphere.normalBuffer;
    _sphereIndexCount = m_sphere.indexCount;
}

std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> GLPBRBaseApp::GetLightPosAndColor() {
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

bool GLPBRBaseApp::initApp() {
    if (!GLCameraBaseApp::initApp()) return false;
    initShapes();
    compileShader(m_sphere.layout);
    return true;
}

void GLPBRBaseApp::compileShader(const rhi::VertexLayout& layout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Base");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "PBR.vs"), "main", false},
                                {rhi::ShaderStage::Fragment, join(shaderDir, "PBR.fs"), "main", false} });
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    m_program = renderer()->createPipeline(layout, shader);
    m_program->setDepthTest(true);
}

std::vector<glm::vec3> GLPBRBaseApp::GenreateObjPos(int radius, float gap, const glm::vec3& center) {
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

void GLPBRBaseApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_sphereVb);
    renderer()->setVertexBuffer(_sphereUv, 1);
    renderer()->setVertexBuffer(_sphereNormal, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    program->setUniformMatrix("normalMatrix", glm::value_ptr(normal), 1, 3);
    renderer()->drawIndexed(_sphereIndexCount, 0, 0);
}

void GLPBRBaseApp::drawScene(const float dt) {
    GLCameraBaseApp::drawScene(dt);
    auto pos = _camera.getAttr().pos;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

    renderer()->setPipeline(m_program);
    m_program->setUniform("texture", 0);
    m_program->setUniform("ao", m_ao);
    m_program->setUniform("projection", glm::value_ptr(projection), 1);
    m_program->setUniform("view", glm::value_ptr(view), 1);
    m_program->setUniform("camPos", glm::value_ptr(pos), 1, 3);
    m_program->setUniform("roughness", m_roughness);
    m_program->setUniform("metallic", m_metallic);
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        m_program->setUniform("albedo", glm::value_ptr(glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f)), 1, 3);
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
        m_program->setUniform("lightPositions[" + std::to_string(i) + "]", glm::value_ptr(lightPositions[i]), 1, 3);
        m_program->setUniform("lightColors[" + std::to_string(i) + "]", glm::value_ptr(lightColors[i]), 1, 3);
        renderSphere(m_program, lightModel);
    }

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
    ImGui::End();
}
