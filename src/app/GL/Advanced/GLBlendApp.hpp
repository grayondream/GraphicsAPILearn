#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLBlendApp : public GLCameraBaseApp {
public:
	virtual ~GLBlendApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	std::shared_ptr<rhi::ITexture2D> _cubeTexture{};
	std::shared_ptr<rhi::ITexture2D> _planeTexture{};
	std::shared_ptr<rhi::ITexture2D> _grassTexture{};
	std::shared_ptr<rhi::ITexture2D> _winTexture{};
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{};
	uint32_t _cubeIndexCount{};
	uint32_t _planeVertexCount{};
	float _curTime{};
	glm::vec3 _objectPosition = glm::vec3(0, 0, -4.0f);
	glm::vec3 _objectScale = glm::vec3(20, 1, 20.0f);
	glm::vec3 _winPos = glm::vec3(0.5f, 0.5f, 5);
	int _grassCount = 4;
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
