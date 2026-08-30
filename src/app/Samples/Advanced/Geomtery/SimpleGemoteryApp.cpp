#include "SimpleGemoteryApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/Common.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;

using namespace ErrorHandle;

SimpleGemoteryApp::~SimpleGemoteryApp() {
}

bool SimpleGemoteryApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}
	
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.vs");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.fs");
	const auto gfile = join(StaticCollector::getGLShaderPath(), "Advanced", "Geometry", "Base.gs");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({{rhi::ShaderStage::Vertex, vfile, "main", false},
	                           {rhi::ShaderStage::Fragment, ffile, "main", false},
	                           {rhi::ShaderStage::Geometry, gfile, "main", false}});
	ErrorHandle::ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	float points[] = {
        -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, // top-left
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, // top-right
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // bottom-right
        -0.5f, -0.5f, 1.0f, 1.0f, 0.0f  // bottom-left
    };
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float2, 0, 0, rhi::VertexInputRate::PerVertex, 0, 5 * static_cast<int>(sizeof(float))});
	layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 2 * static_cast<int>(sizeof(float)), 5 * static_cast<int>(sizeof(float))});
	auto geo = RhiGeometry::CreateFromArray(renderer().get(), points, sizeof(points), 4, layout);
	_vb = geo.vertexBuffer; _vertexCount = geo.vertexCount;
	_pipeline = renderer()->createPipeline(geo.layout, shader);
	_pipeline->setPrimitiveType(rhi::PrimitiveType::Points);
	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void SimpleGemoteryApp::draw(const float dt) {
	ImGui::Begin(rhi::backendDisplayName());
	ImGui::End();

	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_vertexCount, 0);

}
