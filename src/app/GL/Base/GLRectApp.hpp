#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"

class GLRectApp : public GLApp {
public:
	virtual ~GLRectApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

private:
	std::tuple<unsigned int, unsigned int, unsigned int> createVertexBuffer();

private:
	GLProgram _program{};
	unsigned int _vbo{};
	unsigned int _vao{};
	unsigned int _ebo{};
};