#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"
#include "Geometry/Cube.hpp"

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

	void extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view);
	
	void renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view);
	void renderOneCube(GLProgram &program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);
	void renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);

private:
	GLProgram m_bloomProgram;
	GLProgram m_lightProgram;
	unsigned int m_hdrFBO{};
	unsigned int m_colorBuffers[2]{};
	unsigned int m_rboDepth{};
	unsigned int m_pingpongFBO[2]{};
	unsigned int m_pingpongColorbuffers[2]{};

	float _curTime{};	
	
	std::shared_ptr<GLImageTexture2D> m_woodTexture{};
	std::shared_ptr<GLImageTexture2D> m_brickTexture{};
	std::shared_ptr<GLCube> m_cube{};
	std::shared_ptr<GLPlane> m_plane{};
};