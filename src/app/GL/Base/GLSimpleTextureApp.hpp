#pragma once
#include "app/App.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class GLSimpleTextureApp : public App {
public:
	virtual ~GLSimpleTextureApp();
protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt) override;
private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _uv{};
	std::shared_ptr<rhi::IBuffer> _ib{};
	std::shared_ptr<rhi::ITexture2D> _texture{};
	rhi::VertexLayout _layout{};
	uint32_t _indexCount{0};
};
