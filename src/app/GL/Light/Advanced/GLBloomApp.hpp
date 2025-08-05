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
class GLPlane;
class GLCube;
class GLBloomApp : public GLApp {
public:
	virtual ~GLBloomApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void drawScene(const float dt);
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void compileShader();
	void initShapes();
	void createTextures();

private:
	GLProgram _program;

	float _curTime{};
	Camera _camera{};
	bool _clicked{};
	Point2D _lastPos{0.0, 0.0};
	
	bool _enableHdr = true;
	float _exposure = 0.5f;
	std::shared_ptr<GLImageTexture2D> wood_{};
	std::shared_ptr<GLImageTexture2D> brick_{};
	std::shared_ptr<GLCube> cube_{};
	std::shared_ptr<GLPlane> plane_{};
};