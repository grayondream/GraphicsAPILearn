#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"
#include "Geometry/Cube.hpp"

class GLImageTexture2D;
class GLShadowApp : public GLApp {
public:
	virtual ~GLShadowApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void createShadowDepthBuffer();
	void createSphereBuffer();
	void createPlaneBuffer();
	void createScreenBuffer();
	void compileShader();
	
	void renderScene(GLProgram &program, const glm::vec3 &lightPos);

	void renderPlane(GLProgram &program, const glm::mat4 &model);
	void renderCube(GLProgram &program, const glm::mat4 &model, const int type = 1);
	void renderScene2FrameBuffer(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos);


	void renderScene2Screen(const glm::mat4 &lightSpaceMatrix, const glm::vec3 &lightPos);

	void renderDepthDebug();
	
private:
	GLProgram _shadowProgram{};
	GLProgram _depthProgram{};
	GLProgram _debugProgram{};

	bool _enableDepthMap{};
	bool _enableDebug{};
	bool _enableShadowBias{};
	bool _enableCullFace{};
	bool _enableSimplePCF{};

	Cube cube{};
	unsigned int _cubeVbo[3]{};
	unsigned int _cubeVao{};
	unsigned int _cubeEbo{};

	unsigned int _planeVbo[3]{};
	unsigned int _planeVao{};

	unsigned int _shadowDepthMapFbo{};
	unsigned int _shadowDepthMap{};
	unsigned int _screenVao{};
	unsigned int _screenVbo[2];
	unsigned int _screenEbo{};
	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	Point2D _lastPos{0.0, 0.0};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};