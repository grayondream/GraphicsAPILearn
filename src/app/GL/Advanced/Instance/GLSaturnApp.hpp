#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>
#include "geometry/Sphere.hpp"

class Model;
class GLSaturnApp : public GLCameraBaseApp {

public:
	virtual ~GLSaturnApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void loadModel();
	void initSaturnPipeline();
	std::shared_ptr<rhi::IBuffer> generateRockInstanceBuffer(int count);

private:
	std::shared_ptr<rhi::IPipeline> _saturnPipeline{};
	std::shared_ptr<rhi::IPipeline> _rockPipeline{};
	std::shared_ptr<Model> _saturn;
	std::shared_ptr<Model> _rock;
	glm::vec3 _saturnPos{};
	std::shared_ptr<rhi::IBuffer> _instanceBuffer{};
	int _count = 10;
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};