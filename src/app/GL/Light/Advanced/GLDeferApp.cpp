#include "GLDeferApp.hpp"
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
#include <glm/gtc/random.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLDeferApp::~GLDeferApp() {
}

void GLDeferApp::initShapes() {
	auto cubeGeo = RhiGeometry::Create(renderer().get(), m_cube, true, true, true);
	m_cubeVb = cubeGeo.vertexBuffer;
	m_cubeUv = cubeGeo.uvBuffer;
	m_cubeNormal = cubeGeo.normalBuffer;
	m_cubeEbo = cubeGeo.indexBuffer;
	m_cubeIndexCount = cubeGeo.indexCount;

	auto planeGeo = RhiGeometry::Create(renderer().get(), m_plane, true, true, false);
	m_planeVb = planeGeo.vertexBuffer;
	m_planeUv = planeGeo.uvBuffer;
	m_planeNormal = planeGeo.normalBuffer;
	m_planeVertexCount = planeGeo.vertexCount;
	m_cubeLayout = cubeGeo.layout;
}

void GLDeferApp::createQuadBuffer() {
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

void GLDeferApp::createFrameBuffers() {
	const int w = static_cast<int>(m_window->getProperties().width);
	const int h = static_cast<int>(m_window->getProperties().height);
	m_gBuffer = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	// 3 颜色附件，Nearest filter（避免 GBuffer 线性插值模糊）
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA8, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!m_gBuffer->create(fbd)) ExitIfFailed(false, "Failed to create GBuffer framebuffer");
}

bool GLDeferApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	initShapes();
	createQuadBuffer();
	createTextures();
	createFrameBuffers();
	compileShader(m_cubeLayout, m_quadLayout);
	return true;
}

static auto GetPositions(int count = 20, float gap = 0.04f, glm::vec3 center = glm::vec3(0)) {
    std::vector<glm::vec3> cubePositions;

    for (int x = 0; x < count; ++x) {
        for (int z = 0; z < count; ++z) {
            // 计算每个立方体的位置
            float posX = center.x + (x - (count - 1) / 2.0f) * gap;
            float posZ = center.z + (z - (count - 1) / 2.0f) * gap;
            cubePositions.emplace_back(posX, center.y, posZ);
        }
    }

    return cubePositions;
}


static auto GetColorByPos(const glm::vec3& pos) {
    // 将位置坐标归一化到 [0, 1] 范围（可根据实际场景调整映射范围）
    // 示例：假设位置在 [-5, 5] 范围内，先映射到 [0, 1]
    auto normalize = [](float val, float min = -5.0f, float max = 5.0f) {
        return glm::clamp((val - min) / (max - min), 0.0f, 1.0f);
    };

    // x 分量映射到红色，y 映射到绿色，z 映射到蓝色
    float r = normalize(pos.x);       // 红色随 x 变化
    float g = normalize(pos.y);       // 绿色随 y 变化
    float b = normalize(pos.z);       // 蓝色随 z 变化

    // 可添加偏移或缩放增强效果（例如让绿色更明显）
    g = glm::pow(g, 0.8f);  // 绿色曲线调整，使中间值更亮

    return glm::vec3(r, g, b);
}

static std::vector<std::pair<glm::vec3, glm::vec3>> GetLightPosAndColors(int count = 20, float gap = 0.04f, glm::vec3 center = glm::vec3(0)){
	std::vector<std::pair<glm::vec3, glm::vec3>> lightPosAndColors;
	for (int x = 0; x < count; ++x) {
        for (int z = 0; z < count; ++z) {
            // 计算每个立方体的位置
            float posX = center.x + (x - (count - 1) / 2.0f) * gap;
            float posZ = center.z + (z - (count - 1) / 2.0f) * gap;
			const auto pos = glm::vec3(posX, center.y, posZ);
            lightPosAndColors.emplace_back(pos, GetColorByPos(pos));
        }
    }
	return lightPosAndColors;
}

void GLDeferApp::createTextures() {
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

void GLDeferApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Defer");
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "GBuffer.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "GBuffer.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_gBufferProgram = renderer()->createPipeline(cubeLayout, shader);
		m_gBufferProgram->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "LightBox.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "LightBox.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_lightBoxProgram = renderer()->createPipeline(cubeLayout, shader);
		m_lightBoxProgram->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Light.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Light.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		m_lightProgram = renderer()->createPipeline(quadLayout, shader);
		m_lightProgram->setPrimitiveType(rhi::PrimitiveType::TriangleStrip);
	}
}

void GLDeferApp::renderQuad() {
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}

void GLDeferApp::renderOneCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view){
	renderer()->setPipeline(program);
	renderer()->setVertexBuffer(m_cubeVb);
	renderer()->setVertexBuffer(m_cubeUv, 1);
	renderer()->setVertexBuffer(m_cubeNormal, 2);
	renderer()->setIndexBuffer(m_cubeEbo);
	program->setUniform("model", glm::value_ptr(model), 1);
	program->setUniform("projection", glm::value_ptr(projection), 1);
	program->setUniform("view", glm::value_ptr(view), 1);
	renderer()->drawIndexed(m_cubeIndexCount, 0, 0);
}

void GLDeferApp::renderCubes(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
	auto cubePositions = GetPositions(m_Count, 1, glm::vec3(0,0.5,0));
	renderer()->setPipeline(program);
	program->setUniform("viewPos", glm::value_ptr(viewPos), 1, 3);
	renderer()->bindTexture(m_woodTexture, 0);
	program->setUniform("diffuseTexture", 0);
	for (const auto& pos : cubePositions) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, pos);
		model = glm::scale(model, glm::vec3(0.1f));
		renderOneCube(program, model, projection, view);
	}
}

void GLDeferApp::renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos) {
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

void GLDeferApp::renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setPipeline(program);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(0), 0); program->setUniform("gPosition", 0);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(1), 1); program->setUniform("gNormal", 1);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(2), 2); program->setUniform("gAlbedoSpec", 2);
	program->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
	const float linear = 0.7f, quadratic = 1.8f, constant = 1.0f;
	program->setUniform("enableVolume", m_enableVolume);
	auto lightPositionsColor = GetLightPosAndColors(m_Count, 2);
	for (unsigned int i = 0; i < lightPositionsColor.size(); i++) {
		program->setUniform("lights[" + std::to_string(i) + "].Position",
		                    glm::value_ptr(lightPositionsColor[i].first), 1, 3);
		program->setUniform("lights[" + std::to_string(i) + "].Color",
		                    glm::value_ptr(lightPositionsColor[i].second), 1, 3);
		program->setUniform("lights[" + std::to_string(i) + "].Linear", linear);
		program->setUniform("lights[" + std::to_string(i) + "].Quadratic", quadratic);
		const float maxBrightness = std::fmaxf(std::fmaxf(lightPositionsColor[i].second.r, lightPositionsColor[i].second.g), lightPositionsColor[i].second.b);
		float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
		program->setUniform("lights[" + std::to_string(i) + "].Radius", radius);
	}
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}

void GLDeferApp::renderLightBox(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setPipeline(program);
	auto lightPositionsColor = GetLightPosAndColors(m_Count, 1);
	for (const auto& posColor : lightPositionsColor) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, posColor.first);
		model = glm::scale(model, glm::vec3(0.1f));
		program->setUniform("lightColor", glm::value_ptr(posColor.second), 1, 3);
		renderOneCube(program, model, projection, view);
	}
}

void GLDeferApp::renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setRenderTarget(m_gBuffer);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(program);
	program->setUniform("projection", glm::value_ptr(projection), 1);
	program->setUniform("view", glm::value_ptr(view), 1);
	renderer()->bindTexture(m_woodTexture, 0);
	program->setUniform("diffuseTexture", 0);
	renderCubes(program, projection, view, _camera.getAttr().pos);
	renderer()->bindTexture(m_brickTexture, 0);
	program->setUniform("diffuseTexture", 0);
	renderPlane(program, projection, view, _camera.getAttr().pos);
	renderer()->setRenderTarget(nullptr);
}

void GLDeferApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
	renderLight(m_lightProgram, projection, view);

	// 深度-only blit：把 GBuffer 深度拷贝到默认 FBO，供 lightbox 深度测试
	renderer()->blitFramebuffer(m_gBuffer, nullptr, rhi::BlitMask::Depth);

	renderLightBox(m_lightBoxProgram, projection, view);

	ImGui::Begin("OpenGL");
	ImGui::SliderInt("Cube Count", &m_Count, 1, 13);
	ImGui::Checkbox("Enable Volume", &m_enableVolume);
	ImGui::End();
}
