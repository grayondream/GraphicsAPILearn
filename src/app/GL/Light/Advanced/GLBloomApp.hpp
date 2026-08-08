#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class GLImageTexture2D;
class GLPlane;
class GLCube;
class GLBloomApp : public GLCameraBaseApp {
public:
	virtual ~GLBloomApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void initShapes();
	void createTextures();
	void createQuadBuffer();

	void extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view);
	void blurBrightPart();
	void renderFinal();

	void renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view);
	void renderOneCube(GLProgram &program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);
	void renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderQuad();

private:
	GLProgram m_bloomProgram;
	GLProgram m_lightProgram;
	GLProgram m_blurProgram;
	GLProgram m_finalProgram;

	unsigned int m_hdrFBO{};
	unsigned int m_colorBuffers[2]{};
	unsigned int m_rboDepth{};
	unsigned int m_pingpongFBO[2]{};
	unsigned int m_pingpongColorbuffers[2]{};

	float _curTime{};	
	
	bool m_enableBloom{};
	float m_expose{};
	unsigned int m_quadVAO{};
	unsigned int m_quadVBO{};

	std::shared_ptr<GLImageTexture2D> m_woodTexture{};
	std::shared_ptr<GLImageTexture2D> m_brickTexture{};
	std::shared_ptr<GLCube> m_cube{};
	std::shared_ptr<GLPlane> m_plane{};
};