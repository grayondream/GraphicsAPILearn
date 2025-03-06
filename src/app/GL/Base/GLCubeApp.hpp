#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>

class GLImageTexture2D;
class GLCubeApp : public GLApp {
public:
	virtual ~GLCubeApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
};