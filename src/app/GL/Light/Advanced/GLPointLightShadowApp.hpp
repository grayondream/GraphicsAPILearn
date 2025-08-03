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
class GLPointLightShadowApp : public GLApp {
public:
	virtual ~GLPointLightShadowApp();
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
	Camera _camera{};
	bool _clicked{};
	Point2D _lastPos{0.0, 0.0};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};