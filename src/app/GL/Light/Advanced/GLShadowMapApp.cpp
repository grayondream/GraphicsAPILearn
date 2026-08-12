#include "GLShadowMapApp.hpp"
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
#include "geometry/Sphere.hpp"
#include <utils/FileUtils.hpp>
#include <base/Constexpr.hpp>

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLShadowMapApp::~GLShadowMapApp() {
}

bool GLShadowMapApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	initShapes();
	createShadowDepthBuffer();
	createTextures();
	compileShader(_cubeLayout, _screenLayout);
	return true;
}

void GLShadowMapApp::initShapes() {
    Sphere sphere{};
    auto cubeGeo = RhiGeometry::Create(renderer().get(), sphere, false, false, true);
    _cubeVb = cubeGeo.vertexBuffer;
    _cubeEbo = cubeGeo.indexBuffer;
    _cubeIndexCount = cubeGeo.indexCount;
    _cubeLayout = cubeGeo.layout;

    Plane plane{};
    auto planeGeo = RhiGeometry::Create(renderer().get(), plane, true, true, false);
    _planeVb = planeGeo.vertexBuffer;
    _planeUv = planeGeo.uvBuffer;
    _planeNormal = planeGeo.normalBuffer;
    _planeVertexCount = planeGeo.vertexCount;

    // 屏幕 quad（debug 深度可视化）：用 Rect，uv 落在 location 2（Depth.vs 读 textureCoord@2）
    Rect rect{};
    auto rectGeo = RhiGeometry::Create(renderer().get(), rect, true, false, true);
    _screenVb = rectGeo.vertexBuffer;
    _screenUv = rectGeo.uvBuffer;
    _screenEbo = rectGeo.indexBuffer;
    _screenIndexCount = rectGeo.indexCount;
    _screenLayout = rectGeo.layout;
}

void GLShadowMapApp::createTextures() {
    const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
    _texture = RhiImage::Load2D(renderer().get(), imgFile);
    ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);
}

void GLShadowMapApp::createShadowDepthBuffer() {
    _shadowDepthMapFbo = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = Constexpr::GetShadowMapWidth();
    fbd.height = Constexpr::GetShadowMapHeight();
    fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth32F, false, 0,
                               rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
                               rhi::TextureWrap::Repeat, rhi::TextureWrap::Repeat});
    if (!_shadowDepthMapFbo->create(fbd)) ExitIfFailed(false, "Failed to create shadow depth framebuffer");
}

void GLShadowMapApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& screenLayout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "ShadowMap");
    {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "ShadowMapping.vs"), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, "ShadowMapping.fs"), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        _shadowProgram = renderer()->createPipeline(cubeLayout, shader);
        _shadowProgram->setDepthTest(true);
    }
    {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Depth.vs"), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, "Depth.fs"), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        _depthProgram = renderer()->createPipeline(screenLayout, shader);
        _depthProgram->setDepthTest(true);
    }
}

void GLShadowMapApp::renderScene2FrameBuffer() {
    const float near_plane = 1.0f, far_plane = 7.5f;
    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
    glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    renderer()->setPipeline(_shadowProgram);
    _shadowProgram->setUniform("lightSpaceMatrix", glm::value_ptr(lightSpaceMatrix), 1);
    renderer()->setViewport(rhi::Viewport{0, 0, Constexpr::GetShadowMapWidth(), Constexpr::GetShadowMapHeight()});
    renderer()->setRenderTarget(_shadowDepthMapFbo);
    renderer()->clearColor(1.0f, 1.0f, 1.0f, 1.0f);   // RHI clearColor 同时清 depth（GLBackend: glClear(COLOR|DEPTH|STENCIL)）
    {
        renderer()->setVertexBuffer(_planeVb);
        renderer()->setVertexBuffer(_planeUv, 1);
        renderer()->setVertexBuffer(_planeNormal, 2);
        const glm::mat4 identity{1.0f};
        _shadowProgram->setUniform("model", glm::value_ptr(identity), 1);
        renderer()->draw(_planeVertexCount, 0);
    }
    {
        renderer()->setVertexBuffer(_cubeVb);
        renderer()->setIndexBuffer(_cubeEbo);
        auto model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0));
        _shadowProgram->setUniform("model", glm::value_ptr(model), 1);
        renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    }
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}

void GLShadowMapApp::reanderFraemBuffer() {
    renderer()->setPipeline(_depthProgram);
    renderer()->setRenderTarget(nullptr);
    renderer()->clearColor(0.5f, 0.0f, 0.5f, 1.0f);
    renderer()->bindTexture(_shadowDepthMapFbo->depthTexture2D(), 0);
    _depthProgram->setUniform("textureSampler", 0);
    renderer()->setVertexBuffer(_screenVb);
    renderer()->setVertexBuffer(_screenUv, 1);
    renderer()->setIndexBuffer(_screenEbo);
    renderer()->drawIndexed(_screenIndexCount, 0, 0);
}

void GLShadowMapApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::End();

	static float curTime = 0;
	curTime += dt;

	renderScene2FrameBuffer();
	reanderFraemBuffer();
}