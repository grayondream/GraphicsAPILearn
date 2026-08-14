#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLAdvancedGLSLApp : public GLCameraBaseApp {
public:
	virtual ~GLAdvancedGLSLApp();

protected:
	virtual bool initApp();
	virtual void drawScene(const float dt);

private:
	std::shared_ptr<rhi::ITexture2D> _texture{};
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{}, _uv{}, _ebo{};
	uint32_t _indexCount{};
	float _curTime{};
    bool _enablePointSize;
    bool _enableFragCoord;
    bool _enableVertexId;
    bool _enableFrontFaceCulling;
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
