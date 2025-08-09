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
class GLPointLightShadowApp : public GLCameraBaseApp {
public:
	virtual ~GLPointLightShadowApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createShadowDepthBuffer();
	void createCubeBuffer();
	void compileShader();
	
	void renderScene(GLProgram &program, const glm::vec3 &lightPos);
	void renderCube(GLProgram &program, const glm::mat4 &model, const int type = 1);
	void renderScene2FrameBuffer(GLProgram& program, const glm::vec3 &lightPos);


	void renderScene2Screen(GLProgram& program, const glm::vec3 &lightPos);
	
private:
	GLProgram _shadowProgram{};
	GLProgram _depthProgram{};
	bool _enableSimplePCF{};
	bool _enableShadow{};
	float _far = 25.0;
	float _near = 1.0;

	Cube cube{};
	unsigned int _cubeVbo[3]{};
	unsigned int _cubeVao{};
	unsigned int _cubeEbo{};

	unsigned int _shadowDepthMapFbo{};
	unsigned int _shadowDepthMap{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};