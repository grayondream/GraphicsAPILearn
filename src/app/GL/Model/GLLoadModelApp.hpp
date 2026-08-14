#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
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
class GLLoadModelApp : public GLCameraBaseApp {
public:
	virtual ~GLLoadModelApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

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