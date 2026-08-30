#include "LightSourceDirection.hpp"
#include "rhi/core/Common.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

LightSourceDirection::~LightSourceDirection() {
}

bool LightSourceDirection::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "LightSource", "Direction", "Light.vert");
		const auto ffile = join(shaderDir, "LightSource", "Direction", "Light.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto geo = RhiGeometry::Create(renderer().get(), _object, false, false, true);
		_lightPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "LightSource", "Direction", "Object.vert");
		const auto ffile = join(shaderDir, "LightSource", "Direction", "Object.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

		auto geo = RhiGeometry::Create(renderer().get(), _object, true, true, true, {.uvLocation = 3, .normalLocation = 2});
		_vb = geo.vertexBuffer;
		_uv = geo.uvBuffer;
		_normal = geo.normalBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
	}

	{
		const auto objImg = join(StaticCollector::getImagePath(), "container2.jpg");
		_diffuseTex = RhiImage::Load2D(renderer().get(), objImg);
		ExitIfFailed(_diffuseTex != nullptr, "Failed to load texture from file {}", objImg);
	}

	{
		const auto objImg = join(StaticCollector::getImagePath(), "container2_specular.jpg");
		_specularTex = RhiImage::Load2D(renderer().get(), objImg);
		ExitIfFailed(_specularTex != nullptr, "Failed to load texture from file {}", objImg);
	}

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));

	return true;
}

void LightSourceDirection::draw(const float dt) {
	ImGui::Begin(rhi::backendDisplayName());
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::SetNextItemWidth(200);
	static int count{ 1 };
	int cnt = 125;
	ImGui::SliderInt("Cube Count", &count, 1, cnt);
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

	//draw light source（仅 pos+color，用独立管线）
	{
		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
		model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));
		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "lightColor", _lightColor);
		rhi::SetUniform(_ubo, "model", model);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	//draw object（pos+color+normal+uv，normal=2/uv=3，双纹理，多实例）
	{
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_uv, 1);
		renderer()->setVertexBuffer(_normal, 2);

		rhi::SetUniform(_ubo, "projection", projection);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "lightColor", _lightColor);

		glm::vec4 objectColor(1.0f, 0.5f, 0.31f, 1.0f);
		rhi::SetUniform(_ubo, "objectColor", objectColor);

		glm::vec3 lightDir(-0.2f, -1.0f, -0.3f);
		rhi::SetLight(_ubo, 0, "direction", lightDir);
		rhi::SetLightParam(_ubo, 0, "type", 1);

		const auto camPos = _camera.getAttr().pos;
		glm::vec4 viewPos(camPos.x, camPos.y, camPos.z, 1.0f);
		rhi::SetUniform(_ubo, "viewPos", viewPos);

		rhi::SetUniform(_ubo, "material.shininess", 32);

		renderer()->bindTexture(_diffuseTex, 0);
		renderer()->bindTexture(_specularTex, 1);

		glm::vec3 ambientColor(0.2f);
		glm::vec3 diffuseColor(0.5f);
		glm::vec3 specularColor(1.0f);
		rhi::SetLight(_ubo, 0, "ambient", ambientColor);
		rhi::SetLight(_ubo, 0, "diffuse", diffuseColor);
		rhi::SetLight(_ubo, 0, "specular", specularColor);

		std::vector<glm::vec3> cubePositions;
		cubePositions.reserve(cnt);
		int gridSize = 5;
		float spacing = 2.0f;
		glm::vec3 center(0.0f, 0.0f, 0.0f);
		for (int i = 0; i < gridSize; i++) {
			for (int j = 0; j < gridSize; j++) {
				for (int k = 0; k < gridSize; k++) {
					glm::vec3 position(
						center.x + (i - gridSize / 2) * spacing,
						center.y + (j - gridSize / 2) * spacing,
						center.z + (k - gridSize / 2) * spacing - 10
					);
					cubePositions.push_back(position);
				}
			}
		}

		renderer()->setIndexBuffer(_ebo);
		for (int i = 0; i < count; i++) {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, cubePositions[i]);
			float angle = 20.0f * curTime;
			model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			rhi::SetUniform(_ubo, "model", model);
			_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
			renderer()->drawIndexed(_indexCount, 0, 0);
		}
	}
}
