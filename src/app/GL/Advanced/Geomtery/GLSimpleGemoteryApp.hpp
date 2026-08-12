#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>

class GLImageTexture2D;
class GLSimpleGemoteryApp : public GLCameraBaseApp {
public:
	virtual ~GLSimpleGemoteryApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	uint32_t _vertexCount{0};
	float _curTime{};
};
