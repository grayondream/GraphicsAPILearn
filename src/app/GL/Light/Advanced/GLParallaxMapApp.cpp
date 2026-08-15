#include "GLParallaxMapApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
#include "geometry/Rect.hpp"
#include "base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLParallaxMapApp::~GLParallaxMapApp() {
}

static RhiGeometry::Geometry CreateRectBuffer(rhi::IRenderer* renderer) {
	glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
	glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
	glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
	glm::vec3 pos4( 1.0f,  1.0f, 0.0f);
	glm::vec2 uv1(0.0f, 1.0f);
	glm::vec2 uv2(0.0f, 0.0f);
	glm::vec2 uv3(1.0f, 0.0f);
	glm::vec2 uv4(1.0f, 1.0f);
	glm::vec3 nm(0.0f, 0.0f, 1.0f);

	glm::vec3 tangent1, bitangent1;
	glm::vec3 tangent2, bitangent2;
	glm::vec3 edge1 = pos2 - pos1;
	glm::vec3 edge2 = pos3 - pos1;
	glm::vec2 deltaUV1 = uv2 - uv1;
	glm::vec2 deltaUV2 = uv3 - uv1;

	float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
	bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
	bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
	tangent1 = glm::normalize(tangent1);
	bitangent1 = glm::normalize(bitangent1);

	edge1 = pos3 - pos1;
	edge2 = pos4 - pos1;
	deltaUV1 = uv3 - uv1;
	deltaUV2 = uv4 - uv1;
	f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
	bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
	bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
	tangent2 = glm::normalize(tangent2);
	bitangent2 = glm::normalize(bitangent2);

	float quadVertices[] = {
		pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
		pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
		pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z
	};

	constexpr int stride = 14 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 24, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 3, 0, rhi::VertexInputRate::PerVertex, 32, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 4, 0, rhi::VertexInputRate::PerVertex, 44, stride});

	return RhiGeometry::CreateFromArray(renderer, quadVertices, sizeof(quadVertices), 6, layout);
}

bool GLParallaxMapApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!GLCameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	createTextures();
	compileShader();

	auto geo = CreateRectBuffer(renderer().get());
	_vb = geo.vertexBuffer;
	_vertexCount = geo.vertexCount;
	_pipeline = renderer()->createPipeline(geo.layout, _shader);
	_pipeline->setDepthTest(true);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

static std::shared_ptr<rhi::ITexture2D> CreateTexture(rhi::IRenderer* renderer, const std::string& imgname) {
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = RhiImage::Load2D(renderer, imgFile);
	ExitIfFailed(texture != nullptr, "Failed to load texture from file {}", imgFile);
	return texture;
}

void GLParallaxMapApp::createTextures() {
	_brick = CreateTexture(renderer().get(), "bricks2.jpg");
	_brickNormal = CreateTexture(renderer().get(), "bricks2_normal.jpg");
	_brickDisp = CreateTexture(renderer().get(), "bricks2_disp.jpg");
}

void GLParallaxMapApp::compileShader() {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "ParallaxMap");
	_shader = renderer()->createShader();
	const auto vfile = join(shaderDir, "ParallaxMap.vs");
	const auto ffile = join(shaderDir, "ParallaxMap.fs");
	auto ok = _shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                             {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", _shader->getLog());
}

void GLParallaxMapApp::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Normal Map", &_enableDisp);
	ImGui::Checkbox("Enable Steep", &_enableSteep);
	ImGui::Checkbox("Enable Occlusion", &_enableOcclusion);
	ImGui::InputFloat("Height Scale", &_heightScale, 0.1f, 1.0f, "%.2f");
	ImGui::End();

	glm::vec3 lightPos(1.0f, 1.0f, 1.0f);
	const auto attr = _camera.getAttr();
	glm::mat4 projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::mat4 view = _camera.getViewMatrix();
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	rhi::SetUniform(_ubo, "projection", projection);
	rhi::SetUniform(_ubo, "view", view);
	glm::mat4 model = glm::mat4(1.0f);
	rhi::SetUniform(_ubo, "model", model);
	rhi::SetUniform(_ubo, "viewPos", attr.pos);
	rhi::SetUniform(_ubo, "lightPos", lightPos);
	rhi::SetUniform(_ubo, "heightScale", _heightScale);
	rhi::SetUniform(_ubo, "enableDisp", _enableDisp);
	rhi::SetUniform(_ubo, "enableSteep", _enableSteep);
	rhi::SetUniform(_ubo, "enableOcclusion", _enableOcclusion);
	renderer()->bindTexture(_brick, 0);
	renderer()->bindTexture(_brickNormal, 1);
	renderer()->bindTexture(_brickDisp, 2);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_vertexCount, 0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, lightPos);
	model = glm::scale(model, glm::vec3(0.1f));
	rhi::SetUniform(_ubo, "model", model);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->draw(_vertexCount, 0);

}
