#include "GLCubeApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "geometry/Cube.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <utils/FileUtils.hpp>

using FileUtils::join;
using namespace ErrorHandle;

GLCubeApp::~GLCubeApp() {
}

bool GLCubeApp::load(std::shared_ptr<rhi::IRenderer> rhiRenderer) {
	const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "Cube.vert");
	const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "Cube.frag");

	auto shader = renderer()->createShader();
	auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

	const auto imgFile = join(StaticCollector::getImagePath(), "dog.jpg");
	_texture = RhiImage::Load2D(renderer().get(), imgFile);
	ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);

	Cube shape{};
	auto geo = RhiGeometry::Create(renderer().get(), shape, true, true, false);
	_layout = geo.layout;
	_vb = geo.vertexBuffer;
	_uv = geo.uvBuffer;
	_normal = geo.normalBuffer;
	_vertexCount = geo.vertexCount;

	_pipeline = renderer()->createPipeline(_layout, shader);
	_pipeline->setDepthTest(true);

	_uboBuffer = renderer()->createUniformBuffer();
	_uboBuffer->init(nullptr, sizeof(rhi::UniformBlock), rhi::BufferType::Uniform);
	_uboBuffer->bindRange(0, 0, sizeof(rhi::UniformBlock));
	return true;
}

void GLCubeApp::draw(const float dt) {
	renderer()->bindTexture(_texture, 0);
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setVertexBuffer(_uv, 1);
	renderer()->setVertexBuffer(_normal, 2);

	ImGui::Begin("OpenGL");
	static int count{ 1 };
	ImGui::SetNextItemWidth(200);
	ImGui::SliderInt("Cube Count", &count, 1, 10);
	ImGui::End();
	glm::vec3 cubePositions[] = {
	  glm::vec3(0.0f,  0.0f,  0.0f),
	  glm::vec3(2.0f,  5.0f, -15.0f),
	  glm::vec3(-1.5f, -2.2f, -2.5f),
	  glm::vec3(-3.8f, -2.0f, -12.3f),
	  glm::vec3(2.4f, -0.4f, -3.5f),
	  glm::vec3(-1.7f,  3.0f, -7.5f),
	  glm::vec3(1.3f, -2.0f, -2.5f),
	  glm::vec3(1.5f,  2.0f, -2.5f),
	  glm::vec3(1.5f,  0.2f, -1.5f),
	  glm::vec3(-1.3f,  1.0f, -1.5f)
	};

	static float curTime = 0;
	curTime += dt;
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * (i + 1) * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));
		projection = glm::perspective(glm::radians(45.0f), aspectRatio(), 0.1f, 100.0f);
		rhi::SetUniform(_ubo, "model", model);
		rhi::SetUniform(_ubo, "view", view);
		rhi::SetUniform(_ubo, "projection", projection);
		_uboBuffer->update(&_ubo, sizeof(rhi::UniformBlock), 0);
		renderer()->draw(_vertexCount, 0);
	}

}
