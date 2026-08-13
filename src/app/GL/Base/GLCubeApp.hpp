#pragma once
#include "app/App.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>

class GLCubeApp : public App {
public:
	virtual ~GLCubeApp();
protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt) override;
private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _uv{};
	std::shared_ptr<rhi::IBuffer> _normal{};
	std::shared_ptr<rhi::ITexture2D> _texture{};
	rhi::VertexLayout _layout{};
	uint32_t _vertexCount{0};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
