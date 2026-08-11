#include "GLIBLIrradianceConversionApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiImage.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Sphere.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLIBLIrradianceConversionApp::~GLIBLIrradianceConversionApp() {}

void GLIBLIrradianceConversionApp::initShapes() {
    Cube cube{};
    m_cube = RhiGeometry::Create(renderer().get(), cube, true, true, true);
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
}

bool GLIBLIrradianceConversionApp::initApp() {
    if (!GLCameraBaseApp::initApp()) return false;
    compileShader(m_cube.layout);
    initShapes();
    loadTexture();
    initFramebuffer();
    initCaptureViews();
    renderBeforeLoop();
    return true;
}

void GLIBLIrradianceConversionApp::initCaptureViews() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::Linear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = false;
    m_envCubemap = renderer()->createTexture3D();
    m_envCubemap->createEmpty(desc, 512, 512);
}

void GLIBLIrradianceConversionApp::initFramebuffer() {
    m_captureRT = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = 512; fbd.height = 512;
    fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
    if (!m_captureRT->create(fbd)) {
        ExitIfFailed(false, "Failed to create capture framebuffer");
    }
}

void GLIBLIrradianceConversionApp::compileShader(const rhi::VertexLayout& cubeLayout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "IBL_IC");
    auto build = [&](const std::string& vs, const std::string& fs) {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, vs), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, fs), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        return renderer()->createPipeline(cubeLayout, shader);
    };
    m_cubeMapProgram = build("CUBE.vs", "CUBE.fs");
    m_program = build("PBR.vs", "PBR.fs");
    m_backgroundProgram = build("Background.vs", "Background.fs");
    m_program->setDepthTest(true);
    m_cubeMapProgram->setDepthTest(true);
    m_backgroundProgram->setDepthTest(true);
    m_backgroundProgram->setDepthFunc(rhi::CompareFunc::LessEqual);
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

void GLIBLIrradianceConversionApp::loadTexture() {
    const auto resDir = StaticCollector::getImagePath();
    m_hdrEnvTexture = RhiImage::Load2DHDR(renderer().get(), join(resDir, "newport_loft.hdr"));
    ExitIfFailed(m_hdrEnvTexture != nullptr, "Failed to load HDR environment texture");
}

void GLIBLIrradianceConversionApp::renderToCubemap() {
    const auto captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_cubeMapProgram);
    renderer()->bindTexture(m_hdrEnvTexture, 0);
    m_cubeMapProgram->setUniform("equirectangularMap", 0);
    m_cubeMapProgram->setUniform("projection", glm::value_ptr(captureProjection), 1);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const int size = 512;
    for (int i = 0; i < 6; ++i) {
        renderer()->setViewport(rhi::Viewport{0, 0, size, size});
        m_cubeMapProgram->setUniform("view", glm::value_ptr(captureViews[i]), 1);
        m_captureRT->attachCubeFace(m_envCubemap.get(), i);
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderCube(m_cubeMapProgram, glm::mat4(1.0));
    }
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}

void GLIBLIrradianceConversionApp::renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setIndexBuffer(m_cube.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    renderer()->drawIndexed(m_cube.indexCount, 0, 0);
}

void GLIBLIrradianceConversionApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(m_sphere.vertexBuffer);
    renderer()->setVertexBuffer(m_sphere.uvBuffer, 1);
    renderer()->setVertexBuffer(m_sphere.normalBuffer, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    program->setUniformMatrix("normalMatrix", glm::value_ptr(normal), 1, 3);
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

void GLIBLIrradianceConversionApp::renderBeforeLoop() {
    renderToCubemap();
}

void GLIBLIrradianceConversionApp::renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    renderer()->setPipeline(program);
    renderer()->bindTexture(m_envCubemap, 0);
    program->setUniform("view", glm::value_ptr(view), 1);
    program->setUniform("environmentMap", 0);
    program->setUniform("projection", glm::value_ptr(projection), 1);
    renderCube(program, glm::mat4(1.0));
}

void GLIBLIrradianceConversionApp::renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));
    auto pos = _camera.getAttr().pos;
    renderer()->setPipeline(program);
    program->setUniform("texture", 0);
    program->setUniform("projection", glm::value_ptr(projection), 1);
    program->setUniform("view", glm::value_ptr(view), 1);
    program->setUniform("camPos", glm::value_ptr(pos), 1, 3);
    program->setUniform("roughness", m_roughness);
    program->setUniform("metallic", m_metallic);
    program->setUniform("ao", m_ao);
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        program->setUniform("albedo", glm::value_ptr(glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f)), 1, 3);
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(program, objectPos);
    }
    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        program->setUniform("lightPositions[" + std::to_string(i) + "]", glm::value_ptr(lightPositions[i]), 1, 3);
        program->setUniform("lightColors[" + std::to_string(i) + "]", glm::value_ptr(lightColors[i]), 1, 3);
        renderSphere(program, lightModel);
    }
}

void GLIBLIrradianceConversionApp::drawScene(const float dt) {
    GLCameraBaseApp::drawScene(dt);
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
