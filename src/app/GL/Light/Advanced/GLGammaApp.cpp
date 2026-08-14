#include "GLGammaApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Plane.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLGammaApp::~GLGammaApp() {
}

bool GLGammaApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));

	// 光源管线（Sphere shape，pos+color indexed；layout 来自 Create 返回值）
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Advanced", "Gamma", "Source.vs");
		const auto ffile = join(shaderDir, "Advanced", "Gamma", "Source.fs");
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
		const auto vfile = join(shaderDir, "Advanced", "Gamma", "Object.vs");
		const auto ffile = join(shaderDir, "Advanced", "Gamma", "Object.fs");
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
		_texture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);
	}
	return true;
}

void GLGammaApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::Checkbox("Enable Gamma", &_enableGamma);
	ImGui::InputFloat("Gamma Value", &_gammaValue, 0.1f, 10.f, "%.1f");
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	std::vector<glm::vec3> lightPoses = {
		glm::vec3(-2.0f, 0.0f, 0.f),
		glm::vec3(-1.0f, 0.0f, 0.f),
		glm::vec3(-0.0f, 0.0f, 0.f),
		glm::vec3(1.0f, 0.0f, 0.f),
		glm::vec3(2.0f, 0.0f, 0.f)
	};

	std::vector<glm::vec3> lightColors = {
		glm::vec3(1, 0, 0),
		glm::vec3(0, 1, 0),
		glm::vec3(1, 1, 1),
		glm::vec3(0, 0, 1),
		glm::vec3(1, 1, 0),
	};

	//draw object（Plane，drawArrays）
	{
		renderer()->bindTexture(_texture, 0);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::scale(model, glm::vec3(10.f));
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_planeVb);
		renderer()->setVertexBuffer(_planeUv, 1);
		renderer()->setVertexBuffer(_planeNormal, 2);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetUniform(_ubo, "lightColor", _lightColor);
		for (auto i = 0; i < 5; i++) {
			rhi::SetUniform(_ubo, "lightPositions", i, lightPoses[i]);
			rhi::SetUniform(_ubo, "lightColors", i, lightColors[i]);
		}
		rhi::SetUniform(_ubo, "viewPos", _camera.getAttr().pos);
		rhi::SetUniform(_ubo, "enableGamma", _enableGamma);
		rhi::SetUniform(_ubo, "gammaValue", _gammaValue);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->draw(_planeVertexCount, 0);
	}

	//draw light source（Sphere，drawIndexed，5 光源循环）
	for (auto i = 0; i < 5; i++) {
		const auto lightPos = lightPoses[i];
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "model", model);
		const glm::vec4 lc(lightColors[i], 1.0f);
		rhi::SetUniform(_ubo, "lightColor", lc);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
}
