#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Cube.hpp"
#include "app/Samples/RhiGeometry.hpp"

class PointLightShadowApp : public CameraBaseApp {
public:
	virtual ~PointLightShadowApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void createShadowDepthBuffer();
	void initShapes();
	void compileShader(const rhi::VertexLayout& cubeLayout);

	void renderScene(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3 &lightPos);
	void renderCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &model, const int type = 1);
	void renderScene2FrameBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3 &lightPos);

	void renderScene2Screen(std::shared_ptr<rhi::IPipeline>& program, const glm::vec3 &lightPos);

private:
	std::shared_ptr<rhi::IPipeline> _shadowProgram{}, _depthProgram{};
	bool _enableSimplePCF{}, _enableShadow{};
	float _far = 25.0f, _near = 1.0f;

	Cube cube{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeNormal{}, _cubeEbo{};
	uint32_t _cubeIndexCount{0};
	rhi::VertexLayout _cubeLayout{};
	std::shared_ptr<rhi::IRenderTarget> _shadowDepthMapFbo{};
	std::shared_ptr<rhi::ITexture3D> _shadowDepthMap{};
	std::shared_ptr<rhi::ITexture2D> _texture{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};