#pragma once
#include "GLApp.hpp"
#include "Native/GL/GLProgram.hpp"

class GLRectApp : public GLApp {
public:
	virtual ~GLRectApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene();
	virtual void endDrawScene();
	virtual void updateScene(const float dt);

private:
	std::tuple<unsigned int, unsigned int, unsigned int> createVertexBuffer();

private:
	GLProgram _program{};
	unsigned int _vbo{};
	unsigned int _vao{};
	unsigned int _ebo{};
};