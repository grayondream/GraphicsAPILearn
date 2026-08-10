#include "GLBloomApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
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
using FileUtils::join;
using namespace ErrorHandle;

GLBloomApp::~GLBloomApp() {
}

void GLBloomApp::initShapes() {
	auto cubeGeo = RhiGeometry::Create(renderer().get(), m_cube, true, true, true);
	m_cubeVb = cubeGeo.vertexBuffer;
	m_cubeUv = cubeGeo.uvBuffer;
	m_cubeNormal = cubeGeo.normalBuffer;
	m_cubeEbo = cubeGeo.indexBuffer;
	m_cubeIndexCount = cubeGeo.indexCount;
	m_cubeLayout = cubeGeo.layout;

	auto planeGeo = RhiGeometry::Create(renderer().get(), m_plane, true, true, false);
	m_planeVb = planeGeo.vertexBuffer;
	m_planeUv = planeGeo.uvBuffer;
	m_planeNormal = planeGeo.normalBuffer;
	m_planeVertexCount = planeGeo.vertexCount;
}

void GLBloomApp::createQuadBuffer() {
	float quadVertices[] = {
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	constexpr int stride = 5 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	auto geo = RhiGeometry::CreateFromArray(renderer().get(), quadVertices, sizeof(quadVertices), 4, layout);
	m_quadVb = geo.vertexBuffer;
	m_quadVertexCount = geo.vertexCount;
	m_quadLayout = geo.layout;
}

bool GLBloomApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	initShapes();
	createQuadBuffer();
	compileShader(m_cubeLayout, m_quadLayout);
	createTextures();

	const int w = static_cast<int>(m_window->getProperties().width);
	const int h = static_cast<int>(m_window->getProperties().height);
	// HDR FBO：2 个 RGBA16F 颜色附件 + 深度
	m_hdrFBO = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!m_hdrFBO->create(fbd)) {
		ExitIfFailed(false, "Failed to create Hdr framebuffer");
	}
	// pingpong FBO：2 个，各 1 个 RGBA16F 颜色附件，无深度
	for (int i = 0; i < 2; i++) {
		m_pingpongFBO[i] = renderer()->createRenderTarget();
		rhi::FramebufferDesc pbd;
		pbd.width = w; pbd.height = h;
		pbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
		if (!m_pingpongFBO[i]->create(pbd)) {
			ExitIfFailed(false, "Failed to create pingpong framebuffer");
		}
	}

	return true;
}

void GLBloomApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Bloom");
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Light.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Light.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_lightProgram = renderer()->createPipeline(cubeLayout, shader);
		m_lightProgram->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Bloom.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Bloom.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_bloomProgram = renderer()->createPipeline(cubeLayout, shader);
		m_bloomProgram->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Blur.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Blur.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_blurProgram = renderer()->createPipeline(quadLayout, shader);
		m_blurProgram->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Final.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Final.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_finalProgram = renderer()->createPipeline(quadLayout, shader);
		m_finalProgram->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
	}
}

void GLBloomApp::createTextures() {
	const auto resDir = StaticCollector::getImagePath();
	{
		const auto imgFile = join(resDir, "wood.png");
		m_woodTexture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(m_woodTexture != nullptr, "Failed to load texture from file {}", imgFile);
	}
	{
		const auto imgFile = join(resDir, "bricks2.jpg");
		m_brickTexture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(m_brickTexture != nullptr, "Failed to load texture from file {}", imgFile);
	}
}

static auto GetLightPosAndColor(){
	std::vector<glm::vec3> lightPositions;
	lightPositions.push_back(glm::vec3( 0.0f, 0.5f,  1.5f));
	lightPositions.push_back(glm::vec3(-4.0f, 0.5f, -3.0f));
	lightPositions.push_back(glm::vec3( 3.0f, 0.5f,  1.0f));
	lightPositions.push_back(glm::vec3(-.8f,  2.4f, -1.0f));
	std::vector<glm::vec3> lightColors;
	lightColors.push_back(glm::vec3(5.0f,   5.0f,  5.0f));
	lightColors.push_back(glm::vec3(10.0f,  0.0f,  0.0f));
	lightColors.push_back(glm::vec3(0.0f,   0.0f,  15.0f));
	lightColors.push_back(glm::vec3(0.0f,   5.0f,  0.0f));
	return std::pair(lightPositions, lightColors);
}

static auto GetCubePositions() {
	std::vector<glm::vec3> cubePositions = {
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
	return cubePositions;
}

void GLBloomApp::renderOneCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view){
	renderer()->setPipeline(program);
	renderer()->setVertexBuffer(m_cubeVb);
	renderer()->setVertexBuffer(m_cubeUv, 1);
	renderer()->setVertexBuffer(m_cubeNormal, 2);
	renderer()->setIndexBuffer(m_cubeEbo);
	program->setUniform("model", glm::value_ptr(model), 1);
	program->setUniform("projection", glm::value_ptr(projection), 1);
	program->setUniform("view", glm::value_ptr(view), 1);
	renderer()->bindTexture(m_woodTexture, 0);
	program->setUniform("diffuseTexture", 0);
	renderer()->drawIndexed(m_cubeIndexCount, 0, 0);
}

void GLBloomApp::renderCubes(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	auto cubePositions = GetCubePositions();
	renderer()->setPipeline(program);
	program->setUniform("viewPos", glm::value_ptr(viewPos), 1, 3);
	for (const auto& pos : cubePositions) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, pos);
		model = glm::scale(model, glm::vec3(0.5f));
		renderOneCube(program, model, projection, view);
	}
}

void GLBloomApp::renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	renderer()->setPipeline(program);
	renderer()->setVertexBuffer(m_planeVb);
	renderer()->setVertexBuffer(m_planeUv, 1);
	renderer()->setVertexBuffer(m_planeNormal, 2);
	program->setUniform("viewPos", glm::value_ptr(viewPos), 1, 3);
	glm::mat4 model = glm::mat4(1.0f);
	renderer()->bindTexture(m_brickTexture, 0);
	program->setUniform("diffuseTexture", 0);
	model = glm::translate(model, glm::vec3(0.0));
	program->setUniform("model", glm::value_ptr(model), 1);
	program->setUniform("view", glm::value_ptr(view), 1);
	program->setUniform("projection", glm::value_ptr(projection), 1);
	renderer()->draw(m_planeVertexCount, 0);
}

void GLBloomApp::extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setRenderTarget(m_hdrFBO);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderLight(m_lightProgram, projection, view);
	const auto viewPos = _camera.getAttr().pos;
	const auto [lightPositions, lightColors] = GetLightPosAndColor();
	renderer()->setPipeline(m_bloomProgram);
	for (int i = 0; i < lightPositions.size(); i++) {
		m_bloomProgram->setUniform("lights[" + std::to_string(i) + "].Position",
		                           glm::value_ptr(lightPositions[i]), 1, 3);
		m_bloomProgram->setUniform("lights[" + std::to_string(i) + "].Color",
		                           glm::value_ptr(lightColors[i]), 1, 3);
	}
	renderCubes(m_bloomProgram, projection, view, viewPos);
	renderPlane(m_bloomProgram, projection, view, viewPos);
	renderer()->setRenderTarget(nullptr);
}

void GLBloomApp::blurBrightPart() {
	bool horizontal = true, first_iteration = true;
	for (unsigned int i = 0; i < 10; i++) {
		renderer()->setRenderTarget(m_pingpongFBO[horizontal]);
		renderer()->setPipeline(m_blurProgram);
		renderer()->bindTexture(first_iteration ? m_hdrFBO->colorTexture2D(1)
		                                        : m_pingpongFBO[!horizontal]->colorTexture2D(0), 0);
		m_blurProgram->setUniform("image", 0);
		m_blurProgram->setUniform("horizontal", horizontal);
		renderQuad();
		horizontal = !horizontal;
		first_iteration = false;
	}
	renderer()->setRenderTarget(nullptr);
}

void GLBloomApp::renderFinal() {
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(m_finalProgram);
	renderer()->bindTexture(m_hdrFBO->colorTexture2D(0), 0);
	m_finalProgram->setUniform("scene", 0);
	renderer()->bindTexture(m_pingpongFBO[1]->colorTexture2D(0), 1);
	m_finalProgram->setUniform("bloomBlur", 1);
	m_finalProgram->setUniform("bloom", m_enableBloom);
	m_finalProgram->setUniform("exposure", m_expose);
	renderQuad();
}

void GLBloomApp::renderQuad() {
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}

void GLBloomApp::renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection, const glm::mat4& view) {
	auto lightPosAndColor = GetLightPosAndColor();
	const auto& lightPositions = lightPosAndColor.first;
	const auto& lightColors = lightPosAndColor.second;
	renderer()->setPipeline(program);
	for (int i = 0; i < lightPositions.size(); i++) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPositions[i]);
		model = glm::scale(model, glm::vec3(0.25f));
		program->setUniform("lightColor", glm::value_ptr(lightColors[i]), 1, 3);
		renderOneCube(program, model, projection, view);
	}
}

void GLBloomApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	auto pos = _camera.getAttr().pos;
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Bloom", &m_enableBloom);
	ImGui::SliderFloat("Expose Value", &m_expose, 0, 1.0);
	ImGui::End();

	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	extractBrightPart(projection, view);
	blurBrightPart();
	renderFinal();
}
