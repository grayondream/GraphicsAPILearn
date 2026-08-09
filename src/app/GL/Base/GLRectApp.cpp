#include "GLRectApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include <geometry/Rect.hpp>
#include "app/GL/RhiGeometry.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLRectApp::~GLRectApp() {
}

bool GLRectApp::initApp() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "rect.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "rect.frag");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	Rect shape{};
	auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, true);
	_layout = geo.layout;
	_vb = geo.vertexBuffer;
	_ib = geo.indexBuffer;
	_indexCount = geo.indexCount;

	_pipeline = renderer()->createPipeline(_layout, shader);
	return true;
}

void GLRectApp::drawScene(const float dt) {
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setIndexBuffer(_ib);
	renderer()->drawIndexed(_indexCount, 0);
	return GLApp::drawScene(dt);
}
