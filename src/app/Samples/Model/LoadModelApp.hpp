#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class Model;
class LoadModelApp : public CameraBaseApp {
public:
	virtual ~LoadModelApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void loadModel();
	void drawUI();
	void initShader();

private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	float _curTime{};
	std::shared_ptr<Model> _model{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};