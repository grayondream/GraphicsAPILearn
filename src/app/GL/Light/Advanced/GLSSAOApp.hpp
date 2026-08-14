#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>

#include "geometry/Camera.hpp"
#include "model/Model.hpp"

class GLSSAOApp : public GLCameraBaseApp {
public:
	virtual ~GLSSAOApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void initShapes();
	void createTextures();
	void createGBufferFbo();
	void createFrameBuffers();
	void createSSAOFbo();
	void createQuadBuffer();
	void loadModel();
	void initModelPipeline();

	void renderOneCube();
	void renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection, const glm::mat4& view);
	void renderSSAOTexture(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection);
	void renderBlurSSAOTexture(std::shared_ptr<rhi::IPipeline>& program);
	void renderLightPass(std::shared_ptr<rhi::IPipeline>& program);
	void renderQuad();

private:
	struct GBuffer{
		std::shared_ptr<rhi::IRenderTarget> gbuffer{};
		rhi::ITexture2D* gPosition{};
		rhi::ITexture2D* gNormal{};
		rhi::ITexture2D* gAlbedoSpec{};
	};

	struct SSAOBuffer {
		std::shared_ptr<rhi::IRenderTarget> fbo{}, blurFbo{};
		rhi::ITexture2D* ssaoColorBuffer{};
		rhi::ITexture2D* ssaoBlurBuffer{};
		std::shared_ptr<rhi::ITexture2D> noiseTexture{};
	};
private:
	GBuffer m_gBuffer;
	SSAOBuffer m_ssaoBuffer;
	std::shared_ptr<rhi::IPipeline> m_gBufferProgram{}, m_ssaoProgram{}, m_ssaoBlurProgram{}, m_lightProgram{};
	// 几何
	std::shared_ptr<rhi::IBuffer> m_cubeVb{};
	std::shared_ptr<rhi::IBuffer> m_quadVb{};
	uint32_t m_cubeVertexCount{0}, m_quadVertexCount{0};
	rhi::VertexLayout m_cubeLayout{}, m_quadLayout{};
	std::shared_ptr<rhi::IPipeline> m_modelPipeline{};  // 保留（已 RHI）
	std::shared_ptr< Model> m_model{};
	bool m_enableSSAO{};
	float _curTime{};
	rhi::UniformBlock m_ubo{};
	std::shared_ptr<rhi::IBuffer> m_uboBuffer{};
};