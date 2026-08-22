#include "IBLSpecularApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/Common.hpp"
#include "app/Samples/RhiImage.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Sphere.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

IBLSpecularApp::~IBLSpecularApp() {}

static RhiGeometry::Geometry CreateQuadBuffer(rhi::IRenderer* renderer) {
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    constexpr int stride = 5 * static_cast<int>(sizeof(float));
    rhi::VertexLayout layout;
    layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
    layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 12, stride});
    return RhiGeometry::CreateFromArray(renderer, quadVertices, sizeof(quadVertices), 4, layout);
}

static glm::mat4 GetCaptureProjection() {
    return glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
}

void IBLSpecularApp::initShapes() {
    Cube cube{};
    m_cube = RhiGeometry::Create(renderer().get(), cube, true, true, true);
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
    m_quad = CreateQuadBuffer(renderer().get());
}

bool IBLSpecularApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!CameraBaseApp::load(rhiRenderer)) return false;
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    initShapes();
    compileShader(m_cube.layout, m_quad.layout);
    loadTexture();
    initFramebuffer();
    initCaptureViews();
    return true;
}

void IBLSpecularApp::initCaptureViews() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::Linear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;
    m_envCubemap = renderer()->createTexture3D();
    m_envCubemap->createEmpty(desc, 512, 512);
}

void IBLSpecularApp::initFramebuffer() {
    m_captureRT = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = 512; fbd.height = 512;
    fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
    if (!m_captureRT->create(fbd)) {
        ExitIfFailed(false, "Failed to create capture framebuffer");
    }
}

void IBLSpecularApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "IBL_Specular");
    auto build = [&](const rhi::VertexLayout& layout, const std::string& vs, const std::string& fs) {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, vs), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, fs), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        return renderer()->createPipeline(layout, shader);
    };
    m_cubeMapProgram = build(cubeLayout, "CUBE.vs", "CUBE.fs");
    m_program = build(cubeLayout, "PBR.vs", "PBR.fs");
    m_backgroundProgram = build(cubeLayout, "Background.vs", "Background.fs");
    m_irradianceProgram = build(cubeLayout, "Irradiance.vs", "Irradiance.fs");
    m_prefilterProgram = build(cubeLayout, "Prefilter.vs", "Prefilter.fs");
    m_brdfLUTProgram = build(quadLayout, "Brdf.vs", "Brdf.fs");
    m_program->setDepthTest(true);
    m_cubeMapProgram->setDepthTest(true);
    m_backgroundProgram->setDepthTest(true);
    m_irradianceProgram->setDepthTest(true);
    m_prefilterProgram->setDepthTest(true);
    m_backgroundProgram->setDepthFunc(rhi::CompareFunc::LessEqual);
    m_brdfLUTProgram->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
}

static std::vector<glm::mat4> GetCaptureViews() {
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    return std::vector<glm::mat4>(captureViews, captureViews + 6);
}

static auto GetLightPosAndColor() {
    std::vector<glm::vec3> lightPositions = {
        glm::vec3(-10.0f,  10.0f, 10.0f), glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f), glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    std::vector<glm::vec3> lightColors = {
        glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f)
    };
    return std::make_pair(lightPositions, lightColors);
}

void IBLSpecularApp::loadTexture() {
    const auto resDir = StaticCollector::getImagePath();
    m_hdrEnvTexture = RhiImage::Load2DHDR(renderer().get(), join(resDir, "newport_loft.hdr"));
    ExitIfFailed(m_hdrEnvTexture != nullptr, "Failed to load HDR environment texture");
}

void IBLSpecularApp::createIrradianceMap() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::Linear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = false;
    m_irradianceMap = renderer()->createTexture3D();
    m_irradianceMap->createEmpty(desc, 32, 32);
}

void IBLSpecularApp::createPrefilterMap() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::LinearMipLinear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;
    m_prefilterMap = renderer()->createTexture3D();
    m_prefilterMap->createEmpty(desc, 128, 128);
}

void IBLSpecularApp::renderToCubemap() {
    const auto captureProjection = GetCaptureProjection();
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_cubeMapProgram);
    renderer()->bindTexture(m_hdrEnvTexture, 0);
    rhi::SetUniform(_ubo, "projection", captureProjection);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const int size = 512;
    for (int i = 0; i < 6; ++i) {
        renderer()->setRenderTarget(m_captureRT);
        renderer()->setViewport(rhi::Viewport{0, 0, size, size});
        rhi::SetUniform(_ubo, "view", captureViews[i]);
        m_captureRT->attachCubeFace(m_envCubemap.get(), i);
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderCube(m_cubeMapProgram, glm::mat4(1.0));
    }
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(windowWidth()), static_cast<int>(windowHeight())});
}

void IBLSpecularApp::renderIrradianceMap() {
    const auto captureProjection = GetCaptureProjection();
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_irradianceProgram);
    renderer()->bindTexture(m_envCubemap, 0);
    rhi::SetUniform(_ubo, "projection", captureProjection);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const int size = 32;
    for (int i = 0; i < 6; ++i) {
        renderer()->setRenderTarget(m_captureRT);
        renderer()->setViewport(rhi::Viewport{0, 0, size, size});
        rhi::SetUniform(_ubo, "view", captureViews[i]);
        m_captureRT->attachCubeFace(m_irradianceMap.get(), i);
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderCube(m_irradianceProgram, glm::mat4(1.0));
    }
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(windowWidth()), static_cast<int>(windowHeight())});
}

void IBLSpecularApp::renderPerfilterMap() {
    const auto captureProjection = GetCaptureProjection();
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_prefilterProgram);
    renderer()->bindTexture(m_envCubemap, 0);
    rhi::SetUniform(_ubo, "projection", captureProjection);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
        const unsigned int mipSize = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        rhi::SetUniform(_ubo, "roughness", roughness);
        for (int i = 0; i < 6; ++i) {
            renderer()->setRenderTarget(m_captureRT);
            renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(mipSize), static_cast<int>(mipSize)});
            rhi::SetUniform(_ubo, "view", captureViews[i]);
            m_captureRT->attachCubeFace(m_prefilterMap.get(), i, static_cast<int>(mip));
            renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            renderCube(m_prefilterProgram, glm::mat4(1.0));
        }
    }
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(windowWidth()), static_cast<int>(windowHeight())});
}

void IBLSpecularApp::createBrdfLUT() {
    m_brdfLUTRT = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = 512; fbd.height = 512;
    fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RG16F, false, 0});
    if (!m_brdfLUTRT->create(fbd)) {
        ExitIfFailed(false, "Failed to create BRDF LUT framebuffer");
    }
    m_brdfLUTTexture = std::shared_ptr<rhi::ITexture2D>(m_brdfLUTRT->colorTexture2D(0),
                                                        [](rhi::ITexture2D*){});
}

void IBLSpecularApp::renderBrdfLUT() {
    renderer()->setRenderTarget(m_brdfLUTRT);
    renderer()->setViewport(rhi::Viewport{0, 0, 512, 512});
    renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    renderer()->setPipeline(m_brdfLUTProgram);
    renderer()->setVertexBuffer(m_quad.vertexBuffer);
    renderer()->draw(m_quad.vertexCount, 0);
    renderer()->setRenderTarget(nullptr);
    renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(windowWidth()), static_cast<int>(windowHeight())});
}

void IBLSpecularApp::renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    renderer()->setIndexBuffer(m_cube.indexBuffer);
    rhi::SetUniform(_ubo, "model", model);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(m_cube.indexCount, 0, 0);
}

void IBLSpecularApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(m_sphere.vertexBuffer);
    renderer()->setVertexBuffer(m_sphere.uvBuffer, 1);
    renderer()->setVertexBuffer(m_sphere.normalBuffer, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    rhi::SetUniform(_ubo, "model", model);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    rhi::SetUniform(_ubo, "normalMatrix", normal);
    _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
    renderer()->drawIndexed(m_sphere.indexCount, 0, 0);
}

static std::vector<glm::vec3> GenreateObjPos(int radius = 5, float gap = 0.5f, const glm::vec3 &center = glm::vec3(0.0f)) {
    std::vector<glm::vec3> positions;
    if (radius < 0) return positions;
    for (int row = -radius; row <= radius; ++row)
        for (int col = -radius; col <= radius; ++col)
            positions.push_back(glm::vec3(center.x + col * gap, center.y + row * gap, center.z));
    return positions;
}

void IBLSpecularApp::renderBeforeLoop() {
    renderToCubemap();
    renderer()->flush();
    m_envCubemap->genCubeMipmaps();
    createIrradianceMap();
    renderIrradianceMap();
    renderer()->flush();
    createPrefilterMap();
    renderPerfilterMap();
    renderer()->flush();
    createBrdfLUT();
    renderBrdfLUT();
    renderer()->flush();
}

void IBLSpecularApp::renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    renderer()->setPipeline(program);
    renderer()->bindTexture(m_envCubemap, 0);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "projection", projection);
    renderCube(program, glm::mat4(1.0));
}

void IBLSpecularApp::renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));
    auto pos = _camera.getAttr().pos;
    renderer()->setPipeline(program);
    renderer()->bindTexture(m_irradianceMap, 0);
    renderer()->bindTexture(m_prefilterMap, 1);
    renderer()->bindTexture(m_brdfLUTTexture, 2);
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    rhi::SetUniform(_ubo, "camPos", pos);
    rhi::SetUniform(_ubo, "roughness", m_roughness);
    rhi::SetUniform(_ubo, "metallic", m_metallic);
    rhi::SetUniform(_ubo, "ao", m_ao);
    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        rhi::SetUniform(_ubo, "lightPositions", i, lightPositions[i]);
        rhi::SetUniform(_ubo, "lightColors", i, lightColors[i]);
    }
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        rhi::SetUniform(_ubo, "albedo", glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f));
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(program, objectPos);
    }
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        renderSphere(program, lightModel);
    }
}

void IBLSpecularApp::draw(const float dt) {
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderObjectsAndLights(m_program, view, projection);
    renderBackground(m_backgroundProgram, view, projection);

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
    ImGui::End();
}
