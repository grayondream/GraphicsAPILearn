#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/UniformBlock.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Plane.hpp"

class GLBloomApp : public GLCameraBaseApp {
public:
	virtual ~GLBloomApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void initShapes();
	void createTextures();
	void createQuadBuffer();

	void extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view);
	void blurBrightPart();
	void renderFinal();
	void renderQuad();

	void renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& projection, const glm::mat4& view);
	void renderOneCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);
	void renderCubes(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);

private:
	std::shared_ptr<rhi::IPipeline> m_bloomProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightProgram{};
	std::shared_ptr<rhi::IPipeline> m_blurProgram{};
	std::shared_ptr<rhi::IPipeline> m_finalProgram{};

	std::shared_ptr<rhi::IRenderTarget> m_hdrFBO{};
	std::array<std::shared_ptr<rhi::IRenderTarget>, 2> m_pingpongFBO{};

	std::shared_ptr<rhi::ITexture2D> m_woodTexture{};
	std::shared_ptr<rhi::ITexture2D> m_brickTexture{};

	Cube m_cube{};
	Plane m_plane{};
	std::shared_ptr<rhi::IBuffer> m_cubeVb{}, m_cubeUv{}, m_cubeNormal{}, m_cubeEbo{};
	uint32_t m_cubeIndexCount{0};
	std::shared_ptr<rhi::IBuffer> m_planeVb{}, m_planeUv{}, m_planeNormal{};
	uint32_t m_planeVertexCount{0};
	std::shared_ptr<rhi::IBuffer> m_quadVb{};
	uint32_t m_quadVertexCount{0};

	rhi::VertexLayout m_cubeLayout{};
	rhi::VertexLayout m_quadLayout{};

	bool m_enableBloom{};
	float m_expose{};
	rhi::UniformBlock m_ubo{};
	std::shared_ptr<rhi::IBuffer> m_uboBuffer{};
};
