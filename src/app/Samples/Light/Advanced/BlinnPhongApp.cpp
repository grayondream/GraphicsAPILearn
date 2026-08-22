#include "BlinnPhongApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Plane.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

BlinnPhongApp::~BlinnPhongApp() {
}

bool BlinnPhongApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");

	// 光源管线（Sphere shape，pos+color indexed；layout 来自 Create 返回值）
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Source.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Source.fs");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, true);
		_vb = geo.vertexBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_lightPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	// 物体管线（Plane shape，pos+color+uv+normal，drawArrays；默认布局 uv=2/normal=3）
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Object.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Object.fs");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		Plane plane{};
		auto geo = RhiGeometry::Create(renderer().get(), plane, true, true, false);
		_planeVb = geo.vertexBuffer;
		_planeUv = geo.uvBuffer;
		_planeNormal = geo.normalBuffer;
		_planeVertexCount = geo.vertexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
	}

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = RhiImage::Load2D(renderer().get(), imgFile, rhi::TextureWrap::Repeat);  // 平面 UV 0..5 需平铺
		ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);
	}

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void BlinnPhongApp::draw(const float dt) {
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::Checkbox("Enable Blinn Phong", &_enableBlinnPhong);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	const auto lightPos = glm::vec3(-0.0f, 0.0f, 0.f);
	//draw object（Plane，drawArrays）
	{
		renderer()->bindTexture(_texture, 0);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(10.f));
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_planeVb);
		renderer()->setVertexBuffer(_planeUv, 1);
		renderer()->setVertexBuffer(_planeNormal, 2);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetLight(_ubo, 0, "position", lightPos);
		rhi::SetLight(_ubo, 0, "diffuse", glm::vec3(_lightColor));
		rhi::SetUniform(_ubo, "viewPos", _camera.getAttr().pos);
		rhi::SetUniform(_ubo, "enableBlinnPhong", _enableBlinnPhong);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->draw(_planeVertexCount, 0);
	}

	//draw light source（Sphere，drawIndexed）
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetLight(_ubo, 0, "diffuse", glm::vec3(_lightColor));
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

}
