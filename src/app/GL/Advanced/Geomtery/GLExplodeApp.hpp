#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>
#include "geometry/Sphere.hpp"

class GLExplodeApp : public GLCameraBaseApp {
public:
	virtual ~GLExplodeApp();

	protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	Sphere shape{};
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{}, _normal{}, _ebo{};
	uint32_t _indexCount{0};
	float _curTime{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};