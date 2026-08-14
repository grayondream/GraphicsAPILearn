#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"

class GLImageTexture2D;
class GLBlinnPhongApp : public GLCameraBaseApp {
public:
	virtual ~GLBlinnPhongApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	Sphere shape{};
	std::shared_ptr<rhi::IPipeline> _targetPipeline{};
	std::shared_ptr<rhi::IPipeline> _lightPipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _ebo{};
	uint32_t _indexCount{0};
	std::shared_ptr<rhi::IBuffer> _planeVb{};
	std::shared_ptr<rhi::IBuffer> _planeUv{};
	std::shared_ptr<rhi::IBuffer> _planeNormal{};
	uint32_t _planeVertexCount{0};
	bool _enableBlinnPhong{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<rhi::ITexture2D> _texture{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
