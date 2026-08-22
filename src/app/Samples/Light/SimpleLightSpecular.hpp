#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"

class SimpleLightSpecular : public CameraBaseApp {

public:
	virtual ~SimpleLightSpecular();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	Sphere shape{};
	std::shared_ptr<rhi::IPipeline> _targetPipeline{};
	std::shared_ptr<rhi::IPipeline> _lightPipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _normal{};
	std::shared_ptr<rhi::IBuffer> _ebo{};
	uint32_t _indexCount{0};

	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	float _ambientStrength{0.1f};
	float _specularStrength{0.5f};
	float _diffuseStrength{1.0f};
	int _powTimes{32};

	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
