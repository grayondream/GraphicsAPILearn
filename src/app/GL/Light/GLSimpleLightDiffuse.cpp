#include "GLSimpleLightDiffuse.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLSimpleLightDiffuse::~GLSimpleLightDiffuse() {
}

bool GLSimpleLightDiffuse::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Diffuse", "Light.vert");
		const auto ffile = join(shaderDir, "Diffuse", "Light.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, true);
		_lightPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Diffuse", "Object.vert");
		const auto ffile = join(shaderDir, "Diffuse", "Object.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

		auto geo = RhiGeometry::Create(renderer().get(), shape, false, true, true, {.normalLocation = 2});
		_vb = geo.vertexBuffer;
		_normal = geo.normalBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
	}
	return true;
}

void GLSimpleLightDiffuse::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	auto lightPos = glm::vec3(1.0f, 1.0f, 1.50f);
	//draw light source（仅 pos+color，用独立管线）
	{
		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));
		_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
		_lightPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	//draw object（pos+color+normal，normal=2）
	{
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		glm::vec4 objectColor(1.0f, 0.5f, 0.31f, 1.0f);
		glm::vec4 lightPos4(lightPos.x, lightPos.y, lightPos.z, 1.0f);
		_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		_targetPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		_targetPipeline->setUniform("objectColor", glm::value_ptr(objectColor), 1, 4);
		_targetPipeline->setUniform("lightPos", glm::value_ptr(lightPos4), 1, 4);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
}
