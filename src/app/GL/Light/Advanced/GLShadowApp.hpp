#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Cube.hpp"
#include "app/GL/RhiGeometry.hpp"

class GLShadowApp : public GLCameraBaseApp {
public:
	virtual ~GLShadowApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

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

	bool _enableDepthMap{};
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
};