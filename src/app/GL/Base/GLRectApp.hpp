#pragma once
#include "app/GL/GLApp.hpp"
#include "native/GL/GLProgram.hpp"

class GLRectApp : public GLApp {
public:
	virtual ~GLRectApp();

protected:
	virtual bool initApp() override;

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