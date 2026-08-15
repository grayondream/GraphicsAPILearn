#include "GLShadowApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Plane.hpp"
#include "geometry/Rect.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Sphere.hpp"
#include <utils/FileUtils.hpp>
#include <base/Constexpr.hpp>

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLShadowApp::~GLShadowApp() {
}

bool GLShadowApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	_camera = Camera(glm::vec3(0.0f, 3.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90);
	_texture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "wood.png"));
	initShapes();
	createShadowDepthBuffer();
	createQuadBuffer();
	compileShader(_cubeLayout, _quadLayout);
	return true;
}

void GLShadowApp::initShapes() {
    Cube cubeShape{};
    auto cubeGeo = RhiGeometry::Create(renderer().get(), cubeShape, true, true, true);
    _cubeVb = cubeGeo.vertexBuffer; _cubeUv = cubeGeo.uvBuffer;
    _cubeNormal = cubeGeo.normalBuffer; _cubeEbo = cubeGeo.indexBuffer;
    _cubeIndexCount = cubeGeo.indexCount; _cubeLayout = cubeGeo.layout;

    Plane plane{};
    auto planeGeo = RhiGeometry::Create(renderer().get(), plane, true, true, false);
    _planeVb = planeGeo.vertexBuffer; _planeUv = planeGeo.uvBuffer;
    _planeNormal = planeGeo.normalBuffer; _planeVertexCount = planeGeo.vertexCount;
}

void GLShadowApp::createQuadBuffer() {
    // 全屏 quad：pos(vec3)@0 + uv(vec2)@1，4 顶点交错，匹配 DebugQuand.vs
    float quadVertices[] = {
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f
    };
    constexpr int stride = 5 * static_cast<int>(sizeof(float));
    rhi::VertexLayout quadLayout;
    quadLayout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
    quadLayout.elements.push_back({rhi::VertexElement::Float2, 1, 0, rhi::VertexInputRate::PerVertex, 3 * static_cast<int>(sizeof(float)), stride});
    auto quadGeo = RhiGeometry::CreateFromArray(renderer().get(), quadVertices, sizeof(quadVertices), 4, quadLayout);
    _quadVb = quadGeo.vertexBuffer;
    _quadVertexCount = quadGeo.vertexCount;
    _quadLayout = quadGeo.layout;
}

void GLShadowApp::createShadowDepthBuffer() {
    _shadowDepthMapFbo = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = Constexpr::GetShadowMapWidth();
    fbd.height = Constexpr::GetShadowMapHeight();
    rhi::FramebufferAttachment depthAtt;
    depthAtt.type = rhi::AttachmentType::Depth;
    depthAtt.format = rhi::TextureFormat::Depth32F;
    depthAtt.minFilter = rhi::TextureFilter::Nearest;
    depthAtt.magFilter = rhi::TextureFilter::Nearest;
    depthAtt.wrapS = rhi::TextureWrap::ClampToBorder;
    depthAtt.wrapT = rhi::TextureWrap::ClampToBorder;
    depthAtt.borderColor[0] = depthAtt.borderColor[1] = depthAtt.borderColor[2] = depthAtt.borderColor[3] = 1.0f;
    fbd.attachments.push_back(depthAtt);
    if (!_shadowDepthMapFbo->create(fbd)) ExitIfFailed(false, "Failed to create shadow depth framebuffer");
}

void GLShadowApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Shadow");
    auto build = [&](const std::string& vs, const std::string& fs, const rhi::VertexLayout& layout) {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, vs), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, fs), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        return renderer()->createPipeline(layout, shader);
    };
    _shadowProgram = build("ShadowMapping.vs", "ShadowMapping.fs", cubeLayout);
    _depthProgram = build("ShadowMappingDepth.vs", "ShadowMappingDepth.fs", cubeLayout);
    _debugProgram = build("DebugQuand.vs", "DebugQuand.fs", quadLayout);
    _shadowProgram->setDepthTest(true);
    _depthProgram->setDepthTest(true);
    _debugProgram->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
}

void GLShadowApp::renderCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model, const int type) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setVertexBuffer(_cubeNormal, 2);
    renderer()->setIndexBuffer(_cubeEbo);
    rhi::SetUniform(_ubo, "model", model);
    rhi::SetUniform(_ubo, "type", type);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    rhi::SetUniform(_ubo, "type", 0);   // 原生 renderCube 绘后必归 0（GLShadowApp.cpp:245）
}

void GLShadowApp::renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_planeVb);
    renderer()->setVertexBuffer(_planeUv, 1);
    renderer()->setVertexBuffer(_planeNormal, 2);
    rhi::SetUniform(_ubo, "model", model);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->draw(_planeVertexCount, 0);
}

void GLShadowApp::renderScene(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3& lightPos) {
    rhi::SetUniform(_ubo, "debug", _enableDebug ? 1 : 0);
    rhi::SetUniform(_ubo, "enableBias", _enableShadowBias ? 1 : 0);
    rhi::SetUniform(_ubo, "enableSimplePCF", _enableSimplePCF ? 1 : 0);
    auto model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    renderPlane(program, model);
    float scale = 0.25f;
    std::vector<glm::mat4> models;
    models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f)), glm::vec3(scale)));
    models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f)), glm::vec3(scale * 4)));
    models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 1.0f)), glm::vec3(scale * 2)));
    for (auto& m : models) renderCube(program, m, 1);
    auto lightModel = glm::scale(glm::translate(glm::mat4(1.0f), lightPos), glm::vec3(scale));
    renderCube(program, lightModel, 2);
}

void GLShadowApp::renderScene2FrameBuffer(const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos) {
    renderer()->setPipeline(_depthProgram);
    rhi::SetUniform(_ubo, "lightSpaceMatrix", lightSpaceMatrix);
    renderer()->setViewport(rhi::Viewport{0, 0, Constexpr::GetShadowMapWidth(), Constexpr::GetShadowMapHeight()});
    renderer()->setRenderTarget(_shadowDepthMapFbo);
    renderer()->clearColor(1.0f, 1.0f, 1.0f, 1.0f);   // RHI clearColor 同时清 depth
    renderer()->bindTexture(_texture, 0);
    if (_enableCullFace) {
        _depthProgram->setCullFaceEnable(true);
        _depthProgram->setCullFace(rhi::CullFace::Front);
        renderScene(_depthProgram, lightPos);
        _depthProgram->setCullFace(rhi::CullFace::Back);
    } else {
        _depthProgram->setCullFaceEnable(false);
        renderScene(_depthProgram, lightPos);
    }
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, windowWidth(), windowHeight()});
}

void GLShadowApp::renderScene2Screen(const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos) {
    renderer()->setPipeline(_shadowProgram);
    renderer()->bindTexture(_texture, 0);
    renderer()->bindTexture(_shadowDepthMapFbo->depthTexture2D(), 1);
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "viewPos", _camera.getAttr().pos);
    rhi::SetUniform(_ubo, "lightPos", lightPos);
    rhi::SetUniform(_ubo, "lightSpaceMatrix", lightSpaceMatrix);
    renderScene(_shadowProgram, lightPos);
}

void GLShadowApp::renderDepthDebug() {
    const float near_plane = 1.0f, far_plane = 7.5f;
    renderer()->clearColor(1.0f, 1.0f, 1.0f, 1.0f);
    renderer()->setPipeline(_debugProgram);
    rhi::SetUniform(_ubo, "near_plane", near_plane);
    rhi::SetUniform(_ubo, "far_plane", far_plane);
    renderer()->bindTexture(_shadowDepthMapFbo->depthTexture2D(), 0);
    renderer()->setVertexBuffer(_quadVb);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->draw(_quadVertexCount, 0);
}

void GLShadowApp::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Debug", &_enableDebug);
	ImGui::Checkbox("Enable DepthMap", &_enableDepthMap);
	ImGui::Checkbox("Enable Bias", &_enableShadowBias);
	ImGui::Checkbox("Enable CullFace", &_enableCullFace);
	ImGui::Checkbox("Enable SimplePCF", &_enableSimplePCF);

	ImGui::End();

	const float near_plane = 1.0f, far_plane = 7.5f;
	glm::vec3 lightPos = glm::vec3(-1.0f, 3.0f, 1.0f);
	glm::mat4 lightProjection, lightView;
	glm::mat4 lightSpaceMatrix;
	float width = 10;
	lightProjection = glm::ortho(-1 * width, width, -1 * width, width, near_plane, far_plane);
	lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
	lightSpaceMatrix = lightProjection * lightView;

	renderScene2FrameBuffer(lightSpaceMatrix, lightPos);
	renderScene2Screen(lightSpaceMatrix, lightPos);
	if (_enableDepthMap) {
		renderDepthDebug();
	}
}