#include "GLSimpleTextureApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "geometry/Rect.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLSimpleTextureApp::~GLSimpleTextureApp() {
}

bool GLSimpleTextureApp::initApp() {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.frag");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = RhiImage::Load2D(renderer().get(), imgFile);
	ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);

	Rect shape{};
	auto geo = RhiGeometry::Create(renderer().get(), shape, true, false, true);
	_layout = geo.layout;
	_vb = geo.vertexBuffer;
	_uv = geo.uvBuffer;
	_ib = geo.indexBuffer;
	_indexCount = geo.indexCount;

	_pipeline = renderer()->createPipeline(_layout, shader);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLSimpleTextureApp::drawScene(const float dt) {
	renderer()->bindTexture(_texture, 0);
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setVertexBuffer(_uv, 1);
	renderer()->setIndexBuffer(_ib);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(_indexCount, 0);
	return GLApp::drawScene(dt);
}
