#include "AdvancedGLSLApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/Common.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

AdvancedGLSLApp::~AdvancedGLSLApp() {}

bool AdvancedGLSLApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!CameraBaseApp::load(rhiRenderer)) return false;
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "GLSL", "Cube.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "GLSL", "Cube.frag");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
                               {rhi::ShaderStage::Fragment, ffile, "main", false}});
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    Cube cubeShape{};
    auto geo = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
    _vb = geo.vertexBuffer; _uv = geo.uvBuffer; _ebo = geo.indexBuffer;
    _indexCount = geo.indexCount;
    _pipeline = renderer()->createPipeline(geo.layout, shader);
    _texture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "dog.jpg"));
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    return true;
}

void AdvancedGLSLApp::draw(const float dt) {
    ImGui::Begin(rhi::backendDisplayName());
    static int count{ 1 };
    ImGui::Checkbox("Enable Point Size", &_enablePointSize);
    ImGui::Checkbox("Enable Frag Coord", &_enableFragCoord);
    ImGui::Checkbox("Enable Vertex Id", &_enableVertexId);
    ImGui::Checkbox("Enable Front Face Culling", &_enableFrontFaceCulling);
    ImGui::SliderInt("Cube Count", &count, 1, 10);
    ImGui::End();
    if (_enablePointSize) {
        _pipeline->setPolygonMode(rhi::PolygonMode::Point);
        _pipeline->setPointSizeProgramEnable(true);
    } else {
        _pipeline->setPolygonMode(rhi::PolygonMode::Fill);
        _pipeline->setPointSizeProgramEnable(false);
    }
    static float curTime = 0; curTime += dt;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderer()->bindTexture(_texture, 0);
    glm::vec3 cubePositions[10] = {
      glm::vec3(0.0f,  0.0f,  0.0f),
      glm::vec3(2.0f,  5.0f, -15.0f),
      glm::vec3(-1.5f, -2.2f, -2.5f),
      glm::vec3(-3.8f, -2.0f, -12.3f),
      glm::vec3(2.4f, -0.4f, -3.5f),
      glm::vec3(-1.7f,  3.0f, -7.5f),
      glm::vec3(1.3f, -2.0f, -2.5f),
      glm::vec3(1.5f,  2.0f, -2.5f),
      glm::vec3(1.5f,  0.2f, -1.5f),
      glm::vec3(-1.3f,  1.0f, -1.5f)
    };
    for (int i = 0; i < count; i++) {
        renderer()->setPipeline(_pipeline);
        renderer()->setVertexBuffer(_vb);
        renderer()->setVertexBuffer(_uv, 1);
        renderer()->setIndexBuffer(_ebo);
        rhi::SetUniform(_ubo, "projection", projection);
        rhi::SetUniform(_ubo, "view", view);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, cubePositions[i]);
        float angle = 20.0f * (i + 1) * curTime;
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
        rhi::SetUniform(_ubo, "model", model);
        rhi::SetUniform(_ubo, "enablePointSize", _enablePointSize);
        rhi::SetUniform(_ubo, "enableFragCoord", _enableFragCoord);
        rhi::SetUniform(_ubo, "enableVertexId", _enableVertexId);
        rhi::SetUniform(_ubo, "enableFrontFaceCulling", _enableFrontFaceCulling);
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderer()->drawIndexed(_indexCount, 0, 0);
    }
}
