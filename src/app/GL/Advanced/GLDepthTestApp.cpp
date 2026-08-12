#include "GLDepthTestApp.hpp"
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

GLDepthTestApp::~GLDepthTestApp() {}

bool GLDepthTestApp::initApp() {
    if (!GLApp::initApp()) return false;
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "DepthTest", "Basic.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "DepthTest", "Basic.frag");
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
    _contentPipeline = renderer()->createPipeline(cg.layout, shader);
    _contentPipeline->setDepthTest(true);
    _cubeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "marble.jpg"));
    _planeTexture = RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "metal.jpg"));
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

void GLDepthTestApp::drawScene(const float dt) {
    ImGui::Begin("OpenGL"); ImGui::End();
    std::vector<glm::vec3> cubePositions = initializeCubePositions();
    int count = (int)cubePositions.size();
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderer()->bindTexture(_cubeTexture, 0);
    renderer()->setPipeline(_contentPipeline);
    renderer()->setVertexBuffer(_cubeVb);
    renderer()->setVertexBuffer(_cubeUv, 1);
    renderer()->setIndexBuffer(_cubeEbo);
    _contentPipeline->setUniform("projection", glm::value_ptr(projection), 1);
    _contentPipeline->setUniform("view", glm::value_ptr(view), 1);
    _contentPipeline->setUniform("textureSampler", 0);
    for (int i = 0; i < count; i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, cubePositions[i]);
        _contentPipeline->setUniform("model", glm::value_ptr(model), 1);
        renderer()->drawIndexed(_cubeIndexCount, 0, 0);
    }
    renderer()->setVertexBuffer(_planeVb);
    renderer()->setVertexBuffer(_planeUv, 1);
    renderer()->bindTexture(_planeTexture, 1);
    _contentPipeline->setUniform("textureSampler", 1);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, -4.50f, -10));
    _contentPipeline->setUniform("model", glm::value_ptr(model), 1);
    renderer()->draw(_planeVertexCount, 0);
    return GLApp::drawScene(dt);
}
