#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class LightSourceMult : public CameraBaseApp {
public:
	virtual ~LightSourceMult();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void drawUI();
	void drawLight(const glm::mat4& proj, const glm::vec3& pos);
	void drawObjects(const glm::mat4& proj, const float curTime, const std::vector<glm::vec3>& pos);
	void initProgram(const std::string name, std::shared_ptr<rhi::IPipeline>& pipeline);
	std::shared_ptr<rhi::ITexture2D> initTexture(const std::string img);

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

	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
