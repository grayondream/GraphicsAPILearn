#pragma once
#include "app/GL/GLApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>

class GLImageTexture2D;
class GLSimpleTextureApp : public GLApp {
public:
	virtual ~GLSimpleTextureApp();
protected:
	virtual bool initApp() override;

	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	std::array<unsigned int, 2> _vbos{};
	unsigned int _vao{};
	unsigned int _ebo{};
};