#include "GLCullFaceApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLCullFaceApp::~GLCullFaceApp() {}

bool GLCullFaceApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!Sample::load(rhiRenderer)) return false;
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "CullFace", "Basic.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "CullFace", "Basic.frag");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
                               {rhi::ShaderStage::Fragment, ffile, "main", false}});
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    Cube cubeShape{};
    auto cg = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
    _cubeVb = cg.vertexBuffer; _cubeUv = cg.uvBuffer; _cubeEbo = cg.indexBuffer;
    _cubeIndexCount = cg.indexCount;
    _pipeline = renderer()->createPipeline(cg.layout, shader);
    _pipeline->setDepthTest(true);
    _pipeline->setBlend(true);
    _pipeline->setBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
    _pipeline->setCullFaceEnable(true);
    _pipeline->setCullFace(rhi::CullFace::Back);
    _pipeline->setFrontFace(false);
    _cubeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "marble.jpg"));
    _planeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "metal.jpg"));
    _grassTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "grass.png"));
    _winTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "window.png"));
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    return true;
}

void GLCullFaceApp::draw(const float dt) {
    ImGui::Begin("OpenGL");
    ImGui::SetNextItemWidth(200);
    ImGui::SliderInt("Grass Count", &_grassCount, 1, 10);
    ImGui::DragFloat3("Position", &_objectPosition[0], 0.1f);
    ImGui::DragFloat3("Scale", &_objectScale[0], 0.1f);
    ImGui::DragFloat3("Windows Pos", &_winPos[0], 0.1f);
    ImGui::End();

    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderer()->bindTexture(_cubeTexture, 0);
    renderer()->setPipeline(_pipeline);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setIndexBuffer(_cubeEbo);
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0, 0, 0) + _objectPosition);
        model = glm::rotate(model, glm::radians(45.f), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(45.f), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(2.0));
        rhi::SetUniform(_ubo, "texColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
        rhi::SetUniform(_ubo, "model", model);
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    }
}
