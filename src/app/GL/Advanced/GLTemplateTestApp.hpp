#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <memory>
#include "geometry/Camera.hpp"

class GLTemplateTestApp : public GLCameraBaseApp {
public:
	virtual ~GLTemplateTestApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	std::shared_ptr<rhi::ITexture2D> _cubeTexture{};
	std::shared_ptr<rhi::ITexture2D> _planeTexture{};
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IPipeline> _borderPipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{};
	uint32_t _cubeIndexCount{};
	uint32_t _planeVertexCount{};
	float _curTime{};
};
