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
class GLDeferApp : public GLCameraBaseApp {
public:
	virtual ~GLDeferApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void initShapes();
	void createTextures();
	void createFrameBuffers();
	void createQuadBuffer();

	void renderLightBox(GLProgram& program, const glm::mat4& projection, const glm::mat4& view);
	void renderLight(GLProgram& program, const glm::mat4& projection, const glm::mat4& view);
	void renderOneCube(GLProgram &program, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);
	void renderCubes(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderQuad();

	void renderGBuffer(GLProgram &program, const glm::mat4 &projection, const glm::mat4 &view);


private:
	struct GBuffer{
		unsigned int gbuffer;
		unsigned int gPosition;
		unsigned int gNormal;
		unsigned int gAlbedoSpec;
		unsigned int rboDepth;
	};

private:
	GBuffer m_gBuffer;
	GLProgram m_gBufferProgram;
	GLProgram m_lightBoxProgram;
	GLProgram m_lightProgram;

	int m_Count{10};

	unsigned int m_quadVAO{};
	unsigned int m_quadVBO{};
	
	bool m_enableVolume{};
	float _curTime{};	
	
	std::shared_ptr<GLImageTexture2D> m_woodTexture{};
	std::shared_ptr<GLImageTexture2D> m_brickTexture{};
	std::shared_ptr<GLCube> m_cube{};
	std::shared_ptr<GLPlane> m_plane{};
};