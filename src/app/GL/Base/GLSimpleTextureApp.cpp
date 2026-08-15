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

bool GLSimpleTextureApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "SimpleTexture.frag");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = RhiImage::Load2D(renderer().get(), imgFile);
	ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);

	// 用白色顶点：shader 中 color = texture * fragColor，若用默认彩色顶点(Rect 四角红/蓝/绿/白)
	// 插值出的 fragColor 各通道会被调成 0，把纹理乘暗成"隐约可见"。白色顶点不调制纹理。
	Rect shape{
		Vertex{ { 0.5f, 0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
		Vertex{ { 0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
		Vertex{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
		Vertex{ { -0.5f, 0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
	};
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

void GLSimpleTextureApp::draw(const float dt) {
	renderer()->bindTexture(_texture, 0);
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setVertexBuffer(_uv, 1);
	renderer()->setIndexBuffer(_ib);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(_indexCount, 0);
}
