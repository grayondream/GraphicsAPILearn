#pragma once

#include "App/Application.hpp"

class GLApp : public Application {
public:
	GLApp();
	~GLApp();

	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene();
	virtual void endDrawScene();
	virtual void updateScene(const float dt);

private:
	HGLRC CreateOpenGLContext(const HWND winid);

protected:
	HGLRC _glContext{};
	HDC _hdc{};
};
