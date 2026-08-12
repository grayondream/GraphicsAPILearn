#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include <memory>
#include <vector>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLUniformBufferApp : public GLCameraBaseApp {

public:
	virtual ~GLUniformBufferApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	std::vector<std::shared_ptr<rhi::IPipeline>> _pipelines{};
	std::shared_ptr<rhi::IBuffer> _ubo{};
	std::shared_ptr<rhi::IBuffer> _vb{}, _uv{}, _ebo{};
	uint32_t _indexCount{};
	float _curTime{};
};
