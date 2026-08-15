#include "GLMsaaApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Cube.hpp"
#include "geometry/Rect.hpp"
#include "utils/FileUtils.hpp"
using FileUtils::join;

using namespace ErrorHandle;

GLMsaaApp::~GLMsaaApp() {
}

std::shared_ptr<rhi::IPipeline> GLMsaaApp::compileShader(const std::string& name, const rhi::VertexLayout& layout) {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", (name + ".vs"));
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "MSAA", (name + ".fs"));
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
	return renderer()->createPipeline(layout, shader);
}

void GLMsaaApp::compileShader(const rhi::VertexLayout& layout) {
	_pipeline = compileShader("Cube", layout);
	_postPipeline = compileShader("Post", layout);
}

bool GLMsaaApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = RhiImage::Load2D(renderer().get(), imgFile);
	ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);

	Cube cubeShape{};
	auto cubeGeo = RhiGeometry::Create(renderer().get(), cubeShape, true, false, true);
	_cubeVb = cubeGeo.vertexBuffer; _cubeUv = cubeGeo.uvBuffer; _cubeEbo = cubeGeo.indexBuffer;

	Rect rect{};
	auto screenGeo = RhiGeometry::Create(renderer().get(), rect, true, false, true);
	_screenVb = screenGeo.vertexBuffer; _screenUv = screenGeo.uvBuffer; _screenEbo = screenGeo.indexBuffer;

	compileShader(cubeGeo.layout);
	createFrameBuffer();
	createPostFrameBuffer();
	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLMsaaApp::createFrameBuffer() {
	_msaaFbo = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = windowWidth(); fbd.height = windowHeight();
	fbd.samples = 4;
	rhi::FramebufferAttachment color;
	color.type = rhi::AttachmentType::Color;
	color.format = rhi::TextureFormat::RGB8;
	color.samples = 4;
	fbd.attachments.push_back(color);
	rhi::FramebufferAttachment depth;
	depth.type = rhi::AttachmentType::DepthStencil;
	depth.format = rhi::TextureFormat::Depth24Stencil8;
	depth.samples = 4;
	fbd.attachments.push_back(depth);
	if (!_msaaFbo->create(fbd)) ExitIfFailed(false, "Failed to create MSAA framebuffer");
}

void GLMsaaApp::createPostFrameBuffer() {
	_postFbo = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = windowWidth(); fbd.height = windowHeight();
	rhi::FramebufferAttachment color;
	color.type = rhi::AttachmentType::Color;
	color.format = rhi::TextureFormat::RGB8;
	color.minFilter = color.magFilter = rhi::TextureFilter::Linear;
	fbd.attachments.push_back(color);
	if (!_postFbo->create(fbd)) ExitIfFailed(false, "Failed to create post framebuffer");
}

void GLMsaaApp::drawFrameBufferMssa() {
	renderer()->setRenderTarget(_msaaFbo);
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	_pipeline->setDepthTest(true);
	renderer()->setPipeline(_pipeline);
	renderer()->bindTexture(_texture, 0);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	rhi::SetUniform(_ubo, "projection", projection);
	const auto view = _camera.getViewMatrix();
	rhi::SetUniform(_ubo, "view", view);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.0f));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(1.0f, 0.f, 0.f));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(0.0f, 1.f, 0.f));
	rhi::SetUniform(_ubo, "model", model);
	renderer()->setVertexBuffer(_cubeVb);
	renderer()->setVertexBuffer(_cubeUv, 1);
	renderer()->setIndexBuffer(_cubeEbo);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(36, 0, 0);

	renderer()->setViewport(rhi::Viewport{0, 0, windowWidth(), windowHeight()});
	renderer()->blitFramebuffer(_msaaFbo, _postFbo, rhi::BlitMask::Color);
	renderer()->setRenderTarget(nullptr);
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	_postPipeline->setDepthTest(false);
	renderer()->setPipeline(_postPipeline);
	renderer()->bindTexture(_postFbo->colorTexture2D(0), 0);
	renderer()->setVertexBuffer(_screenVb);
	renderer()->setVertexBuffer(_screenUv, 1);
	renderer()->setIndexBuffer(_screenEbo);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(6, 0, 0);
}

void GLMsaaApp::drawGLMssa() {
	renderer()->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
	_pipeline->setDepthTest(true);
	_pipeline->setMultisample(_enableMsaa);
	renderer()->setPipeline(_pipeline);
	renderer()->bindTexture(_texture, 0);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	rhi::SetUniform(_ubo, "projection", projection);
	const auto view = _camera.getViewMatrix();
	rhi::SetUniform(_ubo, "view", view);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.0f));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(1.0f, 0.f, 0.f));
	model = glm::rotate(model, glm::radians(45.f), glm::vec3(0.0f, 1.f, 0.f));
	rhi::SetUniform(_ubo, "model", model);
	renderer()->setVertexBuffer(_cubeVb);
	renderer()->setVertexBuffer(_cubeUv, 1);
	renderer()->setIndexBuffer(_cubeEbo);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->drawIndexed(36, 0, 0);
}

void GLMsaaApp::draw(const float dt) {
	
	ImGui::Begin("OpenGL");
	ImGui::SetNextItemWidth(200);
	ImGui::Checkbox("Enable MSSA", &_enableMsaa);
	ImGui::Checkbox("Enable FrameBuffer MSSA", &_enableFrameBufferMssa);
	ImGui::End();

	if(_enableFrameBufferMssa){
		drawFrameBufferMssa();
	}else {
		drawGLMssa();
	}
	
}
