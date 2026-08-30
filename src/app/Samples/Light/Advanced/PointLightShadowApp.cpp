#include "PointLightShadowApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/Common.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include <utils/FileUtils.hpp>
#include <base/Constexpr.hpp>

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

PointLightShadowApp::~PointLightShadowApp() {
}

bool PointLightShadowApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	_camera = Camera(glm::vec3(-1.f, 0.0f, 1.0f));
	_texture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "wood.png"));
	initShapes();
	createShadowDepthBuffer();
	compileShader(_cubeLayout);
	return true;
}

void PointLightShadowApp::initShapes() {
    Cube cubeShape{};
    auto cubeGeo = RhiGeometry::Create(renderer().get(), cubeShape, true, true, true);
    _cubeVb = cubeGeo.vertexBuffer; _cubeUv = cubeGeo.uvBuffer;
    _cubeNormal = cubeGeo.normalBuffer; _cubeEbo = cubeGeo.indexBuffer;
    _cubeIndexCount = cubeGeo.indexCount; _cubeLayout = cubeGeo.layout;
}

void PointLightShadowApp::createShadowDepthBuffer() {
    rhi::TextureDesc cubemapDesc;
    cubemapDesc.format = rhi::TextureFormat::Depth32F;
    cubemapDesc.wrapS = cubemapDesc.wrapT = cubemapDesc.wrapR = rhi::TextureWrap::ClampToEdge;
    cubemapDesc.minFilter = rhi::TextureFilter::Nearest;
    cubemapDesc.magFilter = rhi::TextureFilter::Nearest;
    cubemapDesc.generateMipmap = false;
    _shadowDepthMap = renderer()->createTexture3D();
    _shadowDepthMap->createEmpty(cubemapDesc, Constexpr::GetShadowMapWidth(), Constexpr::GetShadowMapHeight());

    _shadowDepthMapFbo = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = Constexpr::GetShadowMapWidth();
    fbd.height = Constexpr::GetShadowMapHeight();
    if (!_shadowDepthMapFbo->create(fbd)) ExitIfFailed(false, "Failed to create depth cubemap framebuffer (color base)");
    _shadowDepthMapFbo->attachDepthCube(_shadowDepthMap.get(), 0);
}

void PointLightShadowApp::compileShader(const rhi::VertexLayout& cubeLayout) {
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "PointLightShadow");
    {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "ShadowMapping.vs"), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, "ShadowMapping.fs"), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        _shadowProgram = renderer()->createPipeline(cubeLayout, shader);
        _shadowProgram->setDepthTest(true);
        _shadowProgram->setCullFaceEnable(true);
    }
    {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "ShadowMappingDepth.vs"), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, "ShadowMappingDepth.fs"), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        _depthProgram = renderer()->createPipeline(cubeLayout, shader);
        _depthProgram->setDepthTest(true);
        _depthProgram->setCullFaceEnable(true);
    }
}

void PointLightShadowApp::renderCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model, const int type) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setVertexBuffer(_cubeNormal, 2);
    renderer()->setIndexBuffer(_cubeEbo);
    rhi::SetUniform(_ubo, "model", model);
    rhi::SetUniform(_ubo, "type", type);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    rhi::SetUniform(_ubo, "type", 0);   // 原生 renderCube 绘后归 0，防止后续世界盒被当灯位着色
}

void PointLightShadowApp::renderScene(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3& lightPos) {
    float scale = 0.25f;
    std::vector<glm::mat4> models;
    {   // reversed normals 大世界盒（scale 10）
        auto model = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));
        program->setCullFaceEnable(false);            // 原生 glDisable(GL_CULL_FACE)
        rhi::SetUniform(_ubo, "reverse_normals", 1);
        renderCube(program, model);
        rhi::SetUniform(_ubo, "reverse_normals", 0);
        program->setCullFaceEnable(true);             // 原生 glEnable(GL_CULL_FACE)
    }
    { models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, -3.5f, 0.0)), glm::vec3(0.5f))); }
    { models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 1.0)), glm::vec3(0.75f))); }
    { models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, -1.0f, 0.0)), glm::vec3(0.5f))); }
    { models.push_back(glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 1.0f, 1.5)), glm::vec3(0.5f))); }
    { models.push_back(glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 2.0f, -3.0)),
          glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0))), glm::vec3(0.75f))); }
    for (auto& m : models) renderCube(program, m);
    {   // 灯位
        auto model = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f)), lightPos);   // 原生 scale(0.1) 再 translate → S*T
        rhi::SetUniform(_ubo, "light", 1);
        renderCube(program, model);
        rhi::SetUniform(_ubo, "light", 0);
    }
}

std::vector<glm::mat4> CreateTransformVector(const glm::vec3& lightPos, float aspectRatio, float far_plane, float near_plane){
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspectRatio, near_plane, far_plane);
	std::vector<glm::mat4> shadowTransforms;
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
	return shadowTransforms;
}

void PointLightShadowApp::renderScene2FrameBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3& lightPos) {
    auto shadowTransforms = CreateTransformVector(lightPos, Constexpr::GetShadowMapWidth() * 1.0f / Constexpr::GetShadowMapHeight(), _far, _near);
    renderer()->setPipeline(program);
    renderer()->setViewport(rhi::Viewport{0, 0, Constexpr::GetShadowMapWidth(), Constexpr::GetShadowMapHeight()});
    renderer()->setRenderTarget(_shadowDepthMapFbo);
    rhi::SetUniform(_ubo, "far_plane", _far);
    rhi::SetUniform(_ubo, "lightPos", lightPos);
    for (int face = 0; face < 6; ++face) {
        renderer()->setRenderTarget(_shadowDepthMapFbo);                      // VK: 结束上一面 render pass（重建 framebuffer 前必须）；GL: 重绑 FBO
        _shadowDepthMapFbo->attachCubeFace(_shadowDepthMap.get(), face, 0);   // 逐面 framebuffer（GL 绑定 depth face / VK 单面 view）
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);                       // GL: 清当前面 depth；VK: 记录 clear 值（beginRenderPass 用）
        rhi::SetUniform(_ubo, "shadowMatrices", 0, shadowTransforms[face]);   // 固定槽 extraMat4[1]
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderScene(program, lightPos);
    }
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(windowWidth()), static_cast<int>(windowHeight())});
    renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void PointLightShadowApp::renderScene2Screen(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3& lightPos) {
    renderer()->setPipeline(program);
    // 绑定顺序契约：同 ShadowApp——ClampToBorder 深度 cubemap 先绑，
    // 平铺 wood（Repeat）最后换装 s6，保证本 draw 采样档位正确。
    renderer()->bindTexture(_shadowDepthMap, 1);
    renderer()->bindTexture(_texture, 0);
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "lightPos", lightPos);
    rhi::SetUniform(_ubo, "viewPos", _camera.getAttr().pos);
    rhi::SetUniform(_ubo, "shadows", _enableShadow ? 1 : 0);
    rhi::SetUniform(_ubo, "far_plane", _far);
    renderScene(program, lightPos);
}

void PointLightShadowApp::draw(const float dt) {
	static float curTime = 0;
	curTime += dt;
	glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
	lightPos.z = static_cast<float>(sin(curTime) * 10.0);
	auto pos = _camera.getAttr().pos;
	ImGui::Begin(rhi::backendDisplayName());
	ImGui::Checkbox("Enable PCF", &_enableSimplePCF);
	ImGui::Checkbox("Enable Shadow", &_enableShadow);
	ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Light Pos: (%.2f, %.2f, %.2f)", lightPos.x, lightPos.y, lightPos.z);
	ImGui::End();

	renderScene2FrameBuffer(_depthProgram, lightPos);
	renderScene2Screen(_shadowProgram, lightPos);
}