#pragma once
#include "app/Samples/Base/CameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/UniformBlock.hpp"
#include "app/Samples/RhiGeometry.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class FrameBufferApp : public CameraBaseApp {
public:
	virtual ~FrameBufferApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;
	
private:
	void createFrameBuffer();
	void compileShader(const rhi::VertexLayout& layout);
	void loadTexture();
	void initGLEnv();
	void drawPlane();
	void drawCube();
	std::shared_ptr<rhi::IPipeline> compileShader(const std::string& name, const rhi::VertexLayout& layout);

private:
	std::shared_ptr<rhi::ITexture2D> _cubeTexture{};
	std::shared_ptr<rhi::ITexture2D> _planeTexture{};
	std::shared_ptr<rhi::IPipeline> _contentPipeline{};
	std::shared_ptr<rhi::IPipeline> _screenPipeline{};
	std::shared_ptr<rhi::IBuffer> _cubeVb{}, _cubeUv{}, _cubeEbo{};
	std::shared_ptr<rhi::IBuffer> _planeVb{}, _planeUv{};
	std::shared_ptr<rhi::IBuffer> _screenVb{}, _screenUv{}, _screenEbo{};
	std::shared_ptr<rhi::IRenderTarget> _screenFbo{};
	uint32_t _cubeIndexCount{};
	uint32_t _planeVertexCount{};
	int _selectEffectType{ 0 };
	rhi::UniformBlock _ubo{};
	std::shared_ptr<rhi::IBuffer> _uboBuffer{};
};
