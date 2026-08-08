#pragma once
#include "app/GL/GLApp.hpp"
#include "native/GL/GLProgram.hpp"

class GLTriangleApp : public GLApp {
public:
	virtual ~GLTriangleApp();
protected:
	virtual bool initApp() override;

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