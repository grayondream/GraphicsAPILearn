#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class GLNormalMapApp : public GLCameraBaseApp {
public:
	virtual ~GLNormalMapApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void createTextures();

private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IShader> _shader{};
	uint32_t _vertexCount{0};
	std::shared_ptr<rhi::IBuffer> _vb{};
	bool _enableNormalMap{};
	std::shared_ptr<rhi::ITexture2D> _brick{};
	std::shared_ptr<rhi::ITexture2D> _brickNormal{};
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
