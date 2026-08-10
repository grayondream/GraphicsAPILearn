#include "GLBlinnPhongApp.hpp"
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

GLBlinnPhongApp::~GLBlinnPhongApp() {
}

bool GLBlinnPhongApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
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
		_texture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);
	}
	return true;
}

void GLBlinnPhongApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
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
		_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		_targetPipeline->setUniform("textureSampler", 0);
		_targetPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		_targetPipeline->setUniform("lightPos", glm::value_ptr(lightPos), 1, 3);
		_targetPipeline->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
		_targetPipeline->setUniform("enableBlinnPhong", _enableBlinnPhong);
		renderer()->draw(_planeVertexCount, 0);
	}

	//draw light source（Sphere，drawIndexed）
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
		_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
		_lightPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	return GLApp::drawScene(dt);
}
