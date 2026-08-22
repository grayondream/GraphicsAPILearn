#include "LightSourceMult.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "base/Assert.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include "app/Samples/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

LightSourceMult::~LightSourceMult() {
}

bool LightSourceMult::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	if (!CameraBaseApp::load(rhiRenderer)) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	initProgram("Light", _lightPipeline);
	initProgram("Object", _targetPipeline);
	_diffuseTex = initTexture("container2.jpg");
	_specularTex = initTexture("container2_specular.jpg");

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));

	return true;
}

void LightSourceMult::initProgram(const std::string name, std::shared_ptr<rhi::IPipeline>& pipeline) {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	const auto vfile = join(shaderDir, "LightSource", "Mult", std::string(name + ".vert"));
	const auto ffile = join(shaderDir, "LightSource", "Mult", std::string(name + ".frag"));
	LOGI("Generate program {}", name);
	LOGI("Vertex file : {}", vfile);
	LOGI("Fragment file : {}", ffile);
	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ASSERT(ok, "Failed to create program {}", shader->getLog());
	if (name == "Object") {
		auto geo = RhiGeometry::Create(renderer().get(), _object, true, true, true, {.uvLocation = 3, .normalLocation = 2});
		_vb = geo.vertexBuffer;
		_uv = geo.uvBuffer;
		_normal = geo.normalBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		pipeline = renderer()->createPipeline(geo.layout, shader);
		pipeline->setDepthTest(true);
	} else {
		auto geo = RhiGeometry::Create(renderer().get(), _object, false, false, true);
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		pipeline = renderer()->createPipeline(geo.layout, shader);
	}
}

std::shared_ptr<rhi::ITexture2D> LightSourceMult::initTexture(const std::string img) {
	const auto imgfile = join(StaticCollector::getImagePath(), img);
	auto tex = RhiImage::Load2D(renderer().get(), imgfile);
	LOGI("Initalize texture from file {}", imgfile);
	ASSERT(tex != nullptr, "Failed to initalize texture from {}", imgfile);
	return tex;
}

static std::vector<glm::vec3> GenerateCubePositions(const glm::vec3& center, int gridSize, float spacing) {
	std::vector<glm::vec3> cubePositions;
	cubePositions.reserve(gridSize * gridSize * gridSize);

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

	return cubePositions;
}

static std::vector<glm::vec3> GenerateLightPosition() {
	return {
		{0, 0, -7},
		{2, 2, -10},
		{-2, -2, -8},
		{1, 0, -9},
		{1, 3, -7},
		{0, 0, -5}
	};
}

static int count{ 1 };
void LightSourceMult::drawUI() {
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);

	ImGui::SetNextItemWidth(200);
	int cnt = 125;
	ImGui::SliderInt("Cube Count", &count, 1, cnt);
	ImGui::End();
}

void LightSourceMult::drawLight(const glm::mat4& proj, const glm::vec3& pos) {
	auto view = _camera.getViewMatrix();
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, pos);
	model = glm::rotate(model, 0.f, glm::vec3(1.0f, 0.f, 0.f));
	model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));

	renderer()->setPipeline(_lightPipeline);
	renderer()->setVertexBuffer(_vb);
	rhi::SetUniform(_ubo, "projection", proj);
	rhi::SetUniform(_ubo, "view", view);
	rhi::SetUniform(_ubo, "lightColor", _lightColor);
	rhi::SetUniform(_ubo, "model", model);
	_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
	renderer()->setIndexBuffer(_ebo);
	renderer()->drawIndexed(_indexCount, 0, 0);
}

// 设置方向光参数（简化版）
static void SetDirectionalLight(rhi::UniformBlock& ubo) {
	glm::vec3 direction(-0.2f, -1.0f, -0.3f);
	rhi::SetLight(ubo, 0, "direction", direction);
	rhi::SetLightParam(ubo, 0, "type", 1);
	glm::vec3 ambient(0.05f, 0.05f, 0.05f);
	rhi::SetLight(ubo, 0, "ambient", ambient);
	glm::vec3 diffuse(0.4f, 0.4f, 0.4f);
	rhi::SetLight(ubo, 0, "diffuse", diffuse);
	glm::vec3 specular(0.5f, 0.5f, 0.5f);
	rhi::SetLight(ubo, 0, "specular", specular);
}

// 设置点光源参数（简化版）
static void SetPointLight(rhi::UniformBlock& ubo, int index, const glm::vec3& position) {
	rhi::SetLight(ubo, index + 1, "position", position);
	glm::vec3 ambient(0.05f, 0.05f, 0.05f);
	rhi::SetLight(ubo, index + 1, "ambient", ambient);
	glm::vec3 diffuse(0.8f, 0.8f, 0.8f);
	rhi::SetLight(ubo, index + 1, "diffuse", diffuse);
	glm::vec3 specular(1.0f, 1.0f, 1.0f);
	rhi::SetLight(ubo, index + 1, "specular", specular);
	rhi::SetLightParam(ubo, index + 1, "constant", 1.0f);
	rhi::SetLightParam(ubo, index + 1, "linear", 0.09f);
	rhi::SetLightParam(ubo, index + 1, "quadratic", 0.032f);
}

// 设置聚光灯参数（简化版）
static void SetSpotLight(rhi::UniformBlock& ubo, const glm::vec3& position) {
	rhi::SetLight(ubo, 5, "position", position);
	glm::vec3 direction(0.0f, 0.0f, -1.0f);
	rhi::SetLight(ubo, 5, "direction", direction);
	glm::vec3 ambient(0.0f, 0.0f, 0.0f);
	rhi::SetLight(ubo, 5, "ambient", ambient);
	glm::vec3 diffuse(1.0f, 1.0f, 1.0f);
	rhi::SetLight(ubo, 5, "diffuse", diffuse);
	glm::vec3 specular(1.0f, 1.0f, 1.0f);
	rhi::SetLight(ubo, 5, "specular", specular);
	rhi::SetLightParam(ubo, 5, "constant", 1.0f);
	rhi::SetLightParam(ubo, 5, "linear", 0.09f);
	rhi::SetLightParam(ubo, 5, "quadratic", 0.032f);
	rhi::SetLightParam(ubo, 5, "cutOff", glm::cos(glm::radians(12.5f)));
	rhi::SetLightParam(ubo, 5, "outerCutOff", glm::cos(glm::radians(15.0f)));
}

void LightSourceMult::drawObjects(const glm::mat4& proj, const float curTime, const std::vector<glm::vec3>& lightPoses) {
	auto view = _camera.getViewMatrix();
	int gridSize = 5;
	float spacing = 2.0f;
	glm::vec3 center(0.0f, 0.0f, 0.0f);
	std::vector<glm::vec3> cubePositions = GenerateCubePositions(center, gridSize, spacing);

	renderer()->setPipeline(_targetPipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setVertexBuffer(_uv, 1);
	renderer()->setVertexBuffer(_normal, 2);

	rhi::SetUniform(_ubo, "projection", proj);
	rhi::SetUniform(_ubo, "view", view);
	const auto camPos = _camera.getAttr().pos;
	glm::vec4 viewPos(camPos.x, camPos.y, camPos.z, 1.0f);
	rhi::SetUniform(_ubo, "viewPos", viewPos);
	rhi::SetUniform(_ubo, "material.shininess", 1);

	renderer()->bindTexture(_diffuseTex, 0);
	renderer()->bindTexture(_specularTex, 1);

	SetDirectionalLight(_ubo);
	for (auto i = 0; i < lightPoses.size() - 1; i++) {
		SetPointLight(_ubo, i, lightPoses[i]);
	}
	SetSpotLight(_ubo, lightPoses[lightPoses.size() - 1]);

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

void LightSourceMult::draw(const float dt) {
	drawUI();
	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::vec3 lightPos = glm::vec3(5.0f * sin(curTime), 0.0f, 5.0f * cos(curTime));
	auto lightPoses = GenerateLightPosition();
	for (auto pos : lightPoses) {
		drawLight(projection, pos);
	}

	drawObjects(projection, curTime, lightPoses);
}
