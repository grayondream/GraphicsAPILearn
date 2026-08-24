#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Cube.hpp"
#include "app/Samples/RhiGeometry.hpp"

class ShadowApp : public CameraBaseApp {
public:
	virtual ~ShadowApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void createShadowDepthBuffer();
	void initShapes();
	void createQuadBuffer();
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);

	void renderScene(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3 &lightPos);

	void renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &model);
	void renderCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &model, const int type = 1);
	void renderScene2FrameBuffer(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos);

	void renderScene2Screen(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos);

	void renderDepthDebug();

private:
	std::shared_ptr<rhi::IPipeline> _shadowProgram{}, _depthProgram{}, _debugProgram{};

	bool _enableDepthMap{ std::getenv("RHI_DBG_DEPTHMAP") != nullptr };   // TEMP: 深度图调试取证
	bool _enableDebug{};
	bool _enableShadowBias{};
	bool _enableCullFace{};
	bool _enableSimplePCF{};

	Cube cube{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeNormal{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{}, _planeNormal{};
	std::shared_ptr<rhi::IBuffer> _quadVb{};
	uint32_t _cubeIndexCount{0}, _planeVertexCount{0}, _quadVertexCount{0};
	rhi::VertexLayout _cubeLayout{}, _quadLayout{};
	std::shared_ptr<rhi::IRenderTarget> _shadowDepthMapFbo{};
	std::shared_ptr<rhi::ITexture2D> _texture{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};