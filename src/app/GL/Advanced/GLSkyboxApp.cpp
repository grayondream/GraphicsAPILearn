
#include "GLSkyboxApp.hpp"
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

GLSkyboxApp::~GLSkyboxApp() {}

bool GLSkyboxApp::initApp() {
    if (!GLCameraBaseApp::initApp()) return false;
    Cube cubeShape{};
    auto cg = RhiGeometry::Create(renderer().get(), cubeShape, true, true, true,
                                  RhiGeometry::Layout{3, 2});
    _cubeVb = cg.vertexBuffer; _cubeNormal = cg.normalBuffer;
    _cubeUv = cg.uvBuffer; _cubeEbo = cg.indexBuffer; _cubeIndexCount = cg.indexCount;
    _skyVb = cg.vertexBuffer;
    _skyVertexCount = cg.vertexCount;

    const auto cubeVFile = join(StaticCollector::getGLShaderPath(), "Advanced", "SkyBox", "Cube.vert");
    const auto cubeFFile = join(StaticCollector::getGLShaderPath(), "Advanced", "SkyBox", "Cube.frag");
    auto cubeShader = renderer()->createShader();
    auto cubeOk = cubeShader->compile({{rhi::ShaderStage::Vertex, cubeVFile, "main", false},
                                       {rhi::ShaderStage::Fragment, cubeFFile, "main", false}});
    ExitIfFailed(cubeOk, "Create RHI shader failed: {}", cubeShader->getLog());
    _cubePipeline = renderer()->createPipeline(cg.layout, cubeShader);
    _cubePipeline->setDepthTest(true);

    const auto skyVFile = join(StaticCollector::getGLShaderPath(), "Advanced", "SkyBox", "SkyBox.vert");
    const auto skyFFile = join(StaticCollector::getGLShaderPath(), "Advanced", "SkyBox", "SkyBox.frag");
    auto skyShader = renderer()->createShader();
    auto skyOk = skyShader->compile({{rhi::ShaderStage::Vertex, skyVFile, "main", false},
                                     {rhi::ShaderStage::Fragment, skyFFile, "main", false}});
    ExitIfFailed(skyOk, "Create RHI shader failed: {}", skyShader->getLog());
    rhi::VertexLayout skyLayout;
    skyLayout.elements.push_back({rhi::VertexElement::Float4, 0, 0, rhi::VertexInputRate::PerVertex, 0, 32});
    _skyboxPipeline = renderer()->createPipeline(skyLayout, skyShader);

    const auto imgPath = StaticCollector::getImagePath();
    _texture = RhiImage::Load2D(renderer().get(), join(imgPath, "dog.jpg"));
    _skyBoxTexture = RhiImage::LoadCube(renderer().get(), join(imgPath, "Skybox"));
    ExitIfFailed(_skyBoxTexture != nullptr && _skyBoxTexture->valid(),
                 "Failed to load texture from file {}", join(imgPath, "Skybox"));
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    return true;
}

void GLSkyboxApp::drawCube() {
    renderer()->setPipeline(_cubePipeline);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeNormal, 2);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setIndexBuffer(_cubeEbo);
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    rhi::SetUniform(_ubo, "projection", projection);
    const auto view = _camera.getViewMatrix();
    rhi::SetUniform(_ubo, "view", view);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0, 0, 0));
    model = glm::rotate(model, glm::radians(0.f), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(45.f), glm::vec3(0, 1, 0));
    rhi::SetUniform(_ubo, "model", model);
    auto attr = _camera.getAttr();
    rhi::SetUniform(_ubo, "cameraPos", attr.pos);
    rhi::SetUniform(_ubo, "enableReflection", _enableReflect ? 1.0f : 0.0f);
    rhi::SetUniform(_ubo, "enableRefraction", _enableRefraction ? 1.0f : 0.0f);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(_cubeIndexCount, 0, 0);
}

void GLSkyboxApp::drawSkybox() {
    _skyboxPipeline->setDepthFunc(rhi::CompareFunc::LessEqual);
    _skyboxPipeline->setDepthMask(false);
    renderer()->setPipeline(_skyboxPipeline);
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    rhi::SetUniform(_ubo, "projection", projection);
    auto view = glm::mat4(glm::mat3(_camera.getViewMatrix()));
    rhi::SetUniform(_ubo, "view", view);
    renderer()->bindTexture(_skyBoxTexture, 1);
    renderer()->setVertexBuffer(_skyVb);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->draw(_skyVertexCount, 0);
    _skyboxPipeline->setDepthMask(true);
    _skyboxPipeline->setDepthFunc(rhi::CompareFunc::Less);
}

void GLSkyboxApp::drawScene(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Reflection", &_enableReflect);
	ImGui::Checkbox("Enable Refraction", &_enableRefraction);
	if (_enableReflect && _enableRefraction) {
		_enableReflect = _enableRefraction = false;
	}

	ImGui::End();
	renderer()->bindTexture(_texture, 0);
	renderer()->bindTexture(_skyBoxTexture, 1);
	
	drawCube();
	drawSkybox();
	
	return GLApp::drawScene(dt);
}
