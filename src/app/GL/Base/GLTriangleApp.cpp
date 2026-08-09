#include "GLTriangleApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "geometry/Triangle.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLTriangleApp::~GLTriangleApp() {
}

bool GLTriangleApp::initApp() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.frag");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	Triangle shape{};
	auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, false);
	_layout = geo.layout;
	_vb = geo.vertexBuffer;
	_vertexCount = geo.vertexCount;

	_pipeline = renderer()->createPipeline(_layout, shader);
	return true;
}

void GLTriangleApp::drawScene(const float dt) {
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->draw(_vertexCount, 0);
	return GLApp::drawScene(dt);
}
