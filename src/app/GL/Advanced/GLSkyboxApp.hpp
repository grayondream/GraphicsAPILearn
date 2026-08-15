
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLSkyboxApp : public GLCameraBaseApp {
public:
	virtual ~GLSkyboxApp();

	protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;
	
private:
	void drawCube();
	void drawSkybox();

private:
	std::shared_ptr<rhi::ITexture2D> _texture{};
	std::shared_ptr<rhi::ITexture3D> _skyBoxTexture{};
	std::shared_ptr<rhi::IPipeline> _cubePipeline{};
	std::shared_ptr<rhi::IPipeline> _skyboxPipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeNormal{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _skyVb{};
	uint32_t _cubeIndexCount{};
	uint32_t _skyVertexCount{};
	float _curTime{};
	bool _enableReflect{};
	bool _enableRefraction{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
