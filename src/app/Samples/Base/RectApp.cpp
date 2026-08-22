#include "RectApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
#include <geometry/Rect.hpp>
#include "app/Samples/RhiGeometry.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

RectApp::~RectApp() {
}

bool RectApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
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

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void RectApp::draw(const float dt) {
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setIndexBuffer(_ib);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(_indexCount, 0);
}
