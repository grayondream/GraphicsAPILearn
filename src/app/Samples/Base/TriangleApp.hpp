#pragma once
#include "app/App.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>

class TriangleApp : public App {
public:
	virtual ~TriangleApp();
protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;
private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	rhi::VertexLayout _layout{};
	uint32_t _vertexCount{0};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
