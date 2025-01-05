#pragma once
#include "GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>

class GLImageTexture2D;
class GLSimpleTextureApp : public GLApp {
public:
	virtual ~GLSimpleTextureApp();
public:
	virtual bool init(const HINSTANCE, const WindowDesc& param) override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene();
	virtual void endDrawScene();
	virtual void updateScene(const float dt);

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	std::array<unsigned int, 2> _vbos{};
	unsigned int _vao{};
	unsigned int _ebo{};
};