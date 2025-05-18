#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"

class GLImageTexture2D;
class GLShadowMapApp : public GLApp {
public:
	virtual ~GLShadowMapApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

	virtual void onMouseDown(const UINT msg, WPARAM btnState, int x, int y);
	virtual void onMouseUp(const UINT msg, WPARAM btnState, int x, int y);
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void createShadowDepthBuffer();
	void createVertexBuffer();
	void createPlaneBuffer();
	void createScreenBuffer();
	void compileShader();
	void reanderFraemBuffer();
	void renderScene2FrameBuffer();

private:
	Sphere shape{};
	GLProgram _shadowProgram{};
	GLProgram _depthProgram{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
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
	bool _mouseClicked{ false };
	Point2D _lastPos{0.0, 0.0};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};