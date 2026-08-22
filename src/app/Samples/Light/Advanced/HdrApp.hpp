#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"

class HdrApp : public CameraBaseApp {
public:
	virtual ~HdrApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void render2FrameBuffer();
	void renderHdr();

private:
	std::shared_ptr<rhi::IPipeline> _objPipeline{};
	std::shared_ptr<rhi::IPipeline> _hdrPipeline{};
	std::shared_ptr<rhi::ITexture2D> _brick{};

	std::shared_ptr<rhi::IBuffer> _vb{};            // cube 顶点
	uint32_t _cubeVertexCount{0};
	std::shared_ptr<rhi::IBuffer> _quadVb{};        // 全屏 quad 顶点
	uint32_t _quadVertexCount{0};

	std::shared_ptr<rhi::IRenderTarget> _hdrRT{};
	std::shared_ptr<rhi::ITexture2D> _colorBuffer{};   // = _hdrRT->colorTexture2D(0)

	bool _enableHdr = true;
	float _exposure = 0.5f;
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
