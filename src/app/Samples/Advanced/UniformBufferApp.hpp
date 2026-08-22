#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <vector>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class UniformBufferApp : public CameraBaseApp {

public:
	virtual ~UniformBufferApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	std::vector<std::shared_ptr<rhi::IPipeline>> _pipelines{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _vb{}, _uv{}, _ebo{};
	uint32_t _indexCount{};
	float _curTime{};
};
