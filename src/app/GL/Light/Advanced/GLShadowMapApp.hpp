#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "app/GL/RhiGeometry.hpp"

class GLShadowMapApp : public GLCameraBaseApp {
public:
	virtual ~GLShadowMapApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void createShadowDepthBuffer();
	void initShapes();
	void createTextures();
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& screenLayout);
	void reanderFraemBuffer();
	void renderScene2FrameBuffer();

private:
	std::shared_ptr<rhi::IPipeline> _shadowProgram{};
	std::shared_ptr<rhi::IPipeline> _depthProgram{};
	std::shared_ptr<rhi::IRenderTarget> _shadowDepthMapFbo{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{}, _planeNormal{};
	std::shared_ptr<rhi::IBuffer> _screenVb{}, _screenUv{}, _screenEbo{};
	uint32_t _cubeIndexCount{0}, _planeVertexCount{0}, _screenIndexCount{0};
	rhi::VertexLayout _cubeLayout{}, _screenLayout{};
	std::shared_ptr<rhi::ITexture2D> _texture{};
};