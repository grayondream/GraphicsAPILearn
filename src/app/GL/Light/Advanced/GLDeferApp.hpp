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

class GLDeferApp : public GLCameraBaseApp {
public:
	virtual ~GLDeferApp();

protected:
	virtual bool load(std::shared_ptr<rhi::IRenderer> rhiRenderer) override;
	virtual void draw(const float dt) override;

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void initShapes();
	void createTextures();
	void createFrameBuffers();
	void createQuadBuffer();

	void renderLightBox(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);
	void renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);
	void renderOneCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &model, const glm::mat4 &projection, const glm::mat4 &view);
	void renderCubes(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderQuad();
	void renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);

private:
	std::shared_ptr<rhi::IRenderTarget> m_gBuffer{};

	std::shared_ptr<rhi::IPipeline> m_gBufferProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightBoxProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightProgram{};

	int m_Count{10};

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

	bool m_enableVolume{};
	rhi::UniformBlock m_ubo{};
	std::shared_ptr<rhi::IBuffer> m_uboBuffer{};
};
