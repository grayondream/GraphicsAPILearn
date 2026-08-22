#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class MsaaApp : public CameraBaseApp {
public:
	virtual ~MsaaApp();
	unsigned int getSampleCount() const override { return 4; }
	
protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;
	
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
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
