#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"
#include "model/Model.hpp"

class GLImageTexture2D;
class GLPlane;
class GLCube;
class GLSSAOApp : public GLCameraBaseApp {
public:
	virtual ~GLSSAOApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void initShapes();
	void createTextures();
	void createCubeBuffer();
	void createGBufferFbo();
	void createFrameBuffers();
	void createSSAOFbo();
	void createQuadBuffer();
	void loadModel();

	void renderOneCube();
	void renderGBuffer(GLProgram& program, const glm::mat4& projection, const glm::mat4& view);
	void renderSSAOTexture(GLProgram& program, const glm::mat4& projection);
	void renderBlurSSAOTexture(GLProgram& program);
	void renderLightPass(GLProgram& program);
	void renderQuad();

private:
	struct GBuffer{
		unsigned int gbuffer;
		unsigned int gPosition;
		unsigned int gNormal;
		unsigned int gAlbedoSpec;
		unsigned int rboDepth;
	};

	struct SSAOBuffer {
		unsigned int ssaoColorBuffer;
		unsigned int fbo;
		unsigned int blurFbo;
		unsigned int ssaoBlurBuffer;
		unsigned int noiseTexture;
	};
private:
	GBuffer m_gBuffer;
	SSAOBuffer m_ssaoBuffer;
	GLProgram m_gBufferProgram;
	GLProgram m_ssaoProgram;
	GLProgram m_ssaoBlurProgram;
	GLProgram m_lightProgram;

	unsigned int m_quadVAO{};
	unsigned int m_quadVBO{};
	
	unsigned int m_cubeVAO{};
	unsigned int m_cubeVBO{};

	bool m_enableSSAO{};
	float _curTime{};	
	
	std::shared_ptr<GLImageTexture2D> m_woodTexture{};
	std::shared_ptr<GLImageTexture2D> m_brickTexture{};
	std::shared_ptr< Model> m_model;
};