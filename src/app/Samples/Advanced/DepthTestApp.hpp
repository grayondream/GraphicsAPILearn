#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class DepthTestApp : public CameraBaseApp {

public:
	virtual ~DepthTestApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	std::shared_ptr<rhi::ITexture2D> _cubeTexture{};
	std::shared_ptr<rhi::ITexture2D> _planeTexture{};
	std::shared_ptr<rhi::IPipeline> _contentPipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{};
	uint32_t _cubeIndexCount{};
	uint32_t _planeVertexCount{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
