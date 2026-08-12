#include "GLBlendApp.hpp"
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
#include <geometry/Plane.hpp>
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLBlendApp::~GLBlendApp() {}

bool GLBlendApp::initApp() {
    if (!GLApp::initApp()) return false;
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
    _pipeline = renderer()->createPipeline(cg.layout, shader);
    _pipeline->setDepthTest(true);
    _pipeline->setBlend(true);
    _pipeline->setBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
    _cubeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "marble.jpg"));
    _planeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "metal.jpg"));
    _grassTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "grass.png"));
    _winTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "window.png"));
    return true;
}

static std::vector<glm::vec3> initializeCubePositions() {
	std::vector<glm::vec3> positions;
	float spacing = 1.1f;

	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			for (int z = -2; z < 2; ++z) {
				positions.push_back(glm::vec3(x * spacing, y * spacing - 2, z * spacing - 5));
			}
		}
	}
	return positions;
}

void GLBlendApp::drawScene(const float dt) {
    ImGui::Begin("OpenGL");
    ImGui::SetNextItemWidth(200);
    ImGui::SliderInt("Grass Count", &_grassCount, 1, 10);
    ImGui::DragFloat3("Position", &_objectPosition[0], 0.1f);
    ImGui::DragFloat3("Scale", &_objectScale[0], 0.1f);
    ImGui::DragFloat3("Windows Pos", &_winPos[0], 0.1f);
    ImGui::End();
    std::vector<glm::vec3> cubePositions = initializeCubePositions();
    int count = (int)cubePositions.size();
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderer()->bindTexture(_cubeTexture, 0);
    renderer()->bindTexture(_planeTexture, 1);
    renderer()->bindTexture(_grassTexture, 2);
    renderer()->bindTexture(_winTexture, 3);
    renderer()->setPipeline(_pipeline);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setIndexBuffer(_cubeEbo);
    _pipeline->setUniform("projection", glm::value_ptr(projection), 1);
    _pipeline->setUniform("view", glm::value_ptr(view), 1);
    static float curTime = 0;
    curTime += dt;
    for (int i = 0; i < count; i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, cubePositions[i] + _objectPosition);
        float angle = 0;
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
        _pipeline->setUniform("textureSampler", 1);
        _pipeline->setUniform("texColor", glm::value_ptr(glm::vec4(1.0, 1.0, 1.0, 0.0)), 1, 4);
        _pipeline->setUniform("model", glm::value_ptr(model), 1);
        renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    }
    renderer()->setVertexBuffer(_planeVb);
    renderer()->setVertexBuffer(_planeUv, 1);
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0, -4.50, -10));
        model = glm::scale(model, _objectScale);
        _pipeline->setUniform("model", glm::value_ptr(model), 1);
        _pipeline->setUniform("textureSampler", 0);
        _pipeline->setUniform("texColor", glm::value_ptr(glm::vec4(1.0, 1.0, 1.0, 0.0)), 1, 4);
        renderer()->draw(_planeVertexCount, 0);
    }
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.5f, 0.5f, 0.5f));
        model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        _pipeline->setUniform("model", glm::value_ptr(model), 1);
        _pipeline->setUniform("textureSampler", 2);
        _pipeline->setUniform("texColor", glm::value_ptr(glm::vec4(1.0, 1.0, 1.0, 0.0)), 1, 4);
        renderer()->draw(_planeVertexCount, 0);
    }
    {
        for (int i = 0; i < _grassCount; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5));
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::translate(model, _winPos + glm::vec3(-0.5 * i, 1 * i, 0));
            _pipeline->setUniform("model", glm::value_ptr(model), 1);
            _pipeline->setUniform("textureSampler", 3);
            _pipeline->setUniform("texColor", glm::value_ptr(glm::vec4(1.0, 1.0, 1.0, 0.0)), 1, 4);
            renderer()->draw(_planeVertexCount, 0);
        }
    }
    return GLApp::drawScene(dt);
}
