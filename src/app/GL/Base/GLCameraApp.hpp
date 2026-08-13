#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>


class GLCameraApp : public GLCameraBaseApp {
public:
	virtual ~GLCameraApp();

protected:
	virtual bool initApp() override;

	virtual void drawScene(const float dt);

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
