#include "GLTriangleApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
#include "geometry/Triangle.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLTriangleApp::~GLTriangleApp() {
}

bool GLTriangleApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
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

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLTriangleApp::draw(const float dt) {
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_vertexCount, 0);
}
