#pragma once

#include "App/Application.hpp"

class GLApp : public Application {
public:
	GLApp();
	~GLApp();

	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void onResize(const UINT msg, const WPARAM wParam, const LPARAM lParam);
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

private:
	HGLRC CreateOpenGLContext(const HWND winid);
	bool initGlad();
	void initImGUI();

protected:
	HGLRC _glContext{};
	HDC _hdc{};
};
