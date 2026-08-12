#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLMsaaApp : public GLCameraBaseApp {
public:
	virtual ~GLMsaaApp();
	unsigned int getSampleCount() const override { return 4; }
	
protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createFrameBuffer();
	void createPostFrameBuffer();
	void compileShader(const rhi::VertexLayout& layout);
	void drawGLMssa();
	void drawFrameBufferMssa();
	std::shared_ptr<rhi::IPipeline> compileShader(const std::string& name, const rhi::VertexLayout& layout);

private:
	std::shared_ptr<rhi::ITexture2D> _texture{};
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IPipeline> _postPipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _screenVb{}, _screenUv{}, _screenEbo{};
	std::shared_ptr<rhi::IRenderTarget> _msaaFbo{};
	std::shared_ptr<rhi::IRenderTarget> _postFbo{};
	bool _enableMsaa{ false };
	bool _enableFrameBufferMssa{ false };
};
