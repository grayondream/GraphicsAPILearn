#pragma once
#include "GLApp.hpp"
#include "Native/GL/GLProgram.hpp"

class GLTriangleApp : public GLApp {
public:
	virtual ~GLTriangleApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();
	
private:
	std::pair<unsigned int, unsigned int> createVertexBuffer();

private:
	GLProgram _program{};
	unsigned int _vbo{};
	unsigned int _vao{};
};