#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class GLSimpleLightMap : public GLCameraBaseApp {
public:
	virtual ~GLSimpleLightMap();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	Cube _object{};
	std::shared_ptr<rhi::IPipeline> _targetPipeline{};
	std::shared_ptr<rhi::IPipeline> _lightPipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _uv{};
	std::shared_ptr<rhi::IBuffer> _normal{};
	std::shared_ptr<rhi::IBuffer> _ebo{};
	std::shared_ptr<rhi::ITexture2D> _diffuseTex{};
	std::shared_ptr<rhi::ITexture2D> _specularTex{};
	uint32_t _indexCount{0};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
};
