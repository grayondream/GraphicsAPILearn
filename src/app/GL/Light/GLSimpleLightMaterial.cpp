#include "GLSimpleLightMaterial.hpp"
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

GLSimpleLightMaterial::~GLSimpleLightMaterial() {
}

bool GLSimpleLightMaterial::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Material", "Light.vert");
		const auto ffile = join(shaderDir, "Material", "Light.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, true);
		_lightPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Material", "Object.vert");
		const auto ffile = join(shaderDir, "Material", "Object.frag");
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

void GLSimpleLightMaterial::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	float radius = 5.0f;
	glm::vec3 lightPos = glm::vec3(
		radius * sin(curTime),
		0.0f,
		radius * cos(curTime)
	);
	//draw object（pos+color+normal，normal=2）
	{
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->setIndexBuffer(_ebo);

		int count = 5;
		for (int i = 0; i < count; i++) {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3((i - count / 2) * 2.5, 0.0, 0.0f));
			model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
			glm::vec4 objectColor(1.0f, 0.5f, 0.31f, 1.0f);
			glm::vec4 lightPos4(lightPos.x, lightPos.y, lightPos.z, 1.0f);
			const auto camPos = _camera.getAttr().pos;
			glm::vec4 viewPos4(camPos.x, camPos.y, camPos.z, 1.0f);
			glm::vec4 materialAmbient(1.0f, 0.5f, 0.31f, 1.0f);
			glm::vec4 materialDiffuse(1.0f, 0.5f, 0.31f, 1.0f);
			glm::vec4 materialSpecular(1, 1, 1, 1.0f);
			float v = (i * 1.0 + 1.0) / count;
			glm::vec4 diffuseColor = _lightColor * glm::vec4(v * 3);
			glm::vec4 ambientColor = diffuseColor * glm::vec4(v);
			glm::vec4 lightSpecular = glm::vec4(v * 3);
			_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
			_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
			_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
			_targetPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
			_targetPipeline->setUniform("objectColor", glm::value_ptr(objectColor), 1, 4);
			_targetPipeline->setUniform("light.position", glm::value_ptr(lightPos4), 1, 4);
			_targetPipeline->setUniform("viewPos", glm::value_ptr(viewPos4), 1, 4);
			_targetPipeline->setUniform("material.ambient", glm::value_ptr(materialAmbient), 1, 4);
			_targetPipeline->setUniform("material.diffuse", glm::value_ptr(materialDiffuse), 1, 4);
			_targetPipeline->setUniform("material.specular", glm::value_ptr(materialSpecular), 1, 4);
			_targetPipeline->setUniform("material.shininess", 1.0f);
			_targetPipeline->setUniform("light.ambient", glm::value_ptr(ambientColor), 1, 4);
			_targetPipeline->setUniform("light.diffuse", glm::value_ptr(diffuseColor), 1, 4);
			_targetPipeline->setUniform("light.specular", glm::value_ptr(lightSpecular), 1, 4);
			renderer()->drawIndexed(_indexCount, 0, 0);
		}
	}
}
