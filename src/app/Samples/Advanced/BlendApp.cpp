#include "BlendApp.hpp"
#include "rhi/core/Common.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include "geometry/Plane.hpp"
#include "geometry/Shape.hpp"
#include <utils/FileUtils.hpp>
#include <algorithm>
using FileUtils::join;
using namespace ErrorHandle;

// 竖直透明平面（窗户），1x1 立于 XY 平面，UV 0..1（对应 LearnOpenGL transparentVertices）
class TransparentPlane : public Shape {
public:
	TransparentPlane() {
		const float v[] = {
			// x      y      z    u  v
			0.0f,  0.5f,  0.0f,  0.0f, 0.0f,
			0.0f, -0.5f,  0.0f,  0.0f, 1.0f,
			1.0f, -0.5f,  0.0f,  1.0f, 1.0f,
			0.0f,  0.5f,  0.0f,  0.0f, 0.0f,
			1.0f, -0.5f,  0.0f,  1.0f, 1.0f,
			1.0f,  0.5f,  0.0f,  1.0f, 0.0f};
		for (size_t i = 0; i < sizeof(v) / sizeof(float); i += 5) {
			Vertex vertex;
			vertex.pos = {v[i], v[i + 1], v[i + 2], 1.0f};
			vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
			_normal.push_back({0.0f, 0.0f, 1.0f, 0.0f});
			_pts.push_back(vertex);
			_uv.push_back({v[i + 3], v[i + 4]});
		}
	}
};

BlendApp::~BlendApp() {}

bool BlendApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
    if (!Sample::load(rhiRenderer)) return false;
    _camera.getAttr().pos = glm::vec3(0.0f, 0.0f, 3.0f);
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Blend", "Basic.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Blend", "Basic.frag");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
                               {rhi::ShaderStage::Fragment, ffile, "main", false}});
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    Cube cubeShape{};
    auto cg = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
    _cubeVb = cg.vertexBuffer; _cubeUv = cg.uvBuffer; _cubeEbo = cg.indexBuffer;
    _cubeIndexCount = cg.indexCount;
    Plane plane{};
    auto pg = RhiGeometry::Create(renderer().get(), plane, true, false, false);
    _planeVb = pg.vertexBuffer; _planeUv = pg.uvBuffer; _planeVertexCount = pg.vertexCount;
    TransparentPlane win{};
    auto wg = RhiGeometry::Create(renderer().get(), win, true, false, false);
    _winVb = wg.vertexBuffer; _winUv = wg.uvBuffer; _winVertexCount = wg.vertexCount;
    _pipeline = renderer()->createPipeline(cg.layout, shader);
    _pipeline->setDepthTest(true);
    _pipeline->setBlend(true);
    _pipeline->setBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
    _cubeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "marble.jpg"));
    _floorTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "metal.jpg"),
                                     rhi::TextureWrap::Repeat);  // 地面 UV 0..5 需平铺
    _winTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "window.png"));
    _uboBuffer = renderer()->createUniformBuffer();
    _uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
    _uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
    return true;
}

void BlendApp::draw(const float dt) {
    ImGui::Begin(rhi::backendDisplayName());
    ImGui::SetNextItemWidth(200);
    ImGui::SliderInt("Window Count", &_winCount, 1, 5);
    ImGui::DragFloat3("Position", &_objectPosition[0], 0.1f);
    ImGui::DragFloat3("Windows Pos", &_winPos[0], 0.1f);
    ImGui::End();
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    rhi::SetUniform(_ubo, "projection", projection);
    rhi::SetUniform(_ubo, "view", view);
    renderer()->setPipeline(_pipeline);
    // cubes（不透明物体先绘制）
    static const std::vector<glm::vec3> cubePositions = {
        glm::vec3(-1.0f, 0.0f, -1.0f),
        glm::vec3(2.0f, 0.0f, 0.0f)};
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setIndexBuffer(_cubeEbo);
    for (const auto& pos : cubePositions) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos + _objectPosition);
        renderer()->bindTexture(_cubeTexture, 0);
        rhi::SetUniform(_ubo, "texColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
        rhi::SetUniform(_ubo, "model", model);
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    }
    // floor（不透明，Plane 顶点已编码 y=-0.5 与 XZ±5）
    renderer()->setVertexBuffer(_planeVb);
    renderer()->setVertexBuffer(_planeUv, 1);
    {
        glm::mat4 model = glm::mat4(1.0f);
        renderer()->bindTexture(_floorTexture, 0);
        rhi::SetUniform(_ubo, "model", model);
        rhi::SetUniform(_ubo, "texColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderer()->draw(_planeVertexCount, 0);
    }
    // windows（透明物体最后绘制，按相机距离从远到近）
    static const std::vector<glm::vec3> winPositions = {
        glm::vec3(-1.5f, 0.0f, -0.48f),
        glm::vec3(1.5f, 0.0f, 0.51f),
        glm::vec3(0.0f, 0.0f, 0.7f),
        glm::vec3(-0.3f, 0.0f, -2.3f),
        glm::vec3(0.5f, 0.0f, -0.6f)};
    std::vector<glm::vec3> sorted;
    for (const auto& p : winPositions) {
        glm::vec3 w = p + _winPos;
        sorted.push_back(w);
    }
    std::sort(sorted.begin(), sorted.end(), [this](const glm::vec3& a, const glm::vec3& b) {
        return glm::length(_camera.getAttr().pos - a) > glm::length(_camera.getAttr().pos - b);
    });
    renderer()->setVertexBuffer(_winVb);
    renderer()->setVertexBuffer(_winUv, 1);
    for (int i = 0; i < _winCount; i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, sorted[i]);
        renderer()->bindTexture(_winTexture, 0);
        rhi::SetUniform(_ubo, "model", model);
        rhi::SetUniform(_ubo, "texColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
        _uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
        renderer()->draw(_winVertexCount, 0);
    }
}
