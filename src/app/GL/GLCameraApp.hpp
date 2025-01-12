#pragma once
#include "GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"

class GLImageTexture2D;
class GLCameraApp : public GLApp {
public:
	virtual ~GLCameraApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene();
	virtual void endDrawScene();
	virtual void updateScene(const float dt);

	virtual void onMouseDown(WPARAM btnState, int x, int y);
	virtual void onMouseUp(WPARAM btnState, int x, int y);
	virtual void onMouseMove(WPARAM btnState, int x, int y);
	virtual void onMouseScroll(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void onKeyBoardEvent(const UINT msg, const WPARAM wParam, const LPARAM lParam) override;

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	float _curTime{};
	Camera _camera{};
};