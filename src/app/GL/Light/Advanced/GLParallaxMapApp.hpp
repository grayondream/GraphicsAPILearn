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
class GLParallaxMapApp : public GLApp {
public:
	virtual ~GLParallaxMapApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void drawScene(const float dt);
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void compileShader();
	void createTextures();

private:
	GLProgram _program;

	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	Point2D _lastPos{0.0, 0.0};
	unsigned int _vao{};
	unsigned int _vbo{};
	bool _enableDisp{};
	float _heightScale{0.1};
	std::shared_ptr<GLImageTexture2D> _brick{};
	std::shared_ptr<GLImageTexture2D> _brickNormal{};
	std::shared_ptr<GLImageTexture2D> _brickDisp{};
};