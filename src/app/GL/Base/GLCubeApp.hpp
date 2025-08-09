#pragma once
#include "App/GL/GLApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>

class GLImageTexture2D;
class GLCube;
class GLCubeApp : public GLApp {
public:
	virtual ~GLCubeApp();

protected:
	virtual bool initApp() override;

	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

private:
	void initializeCube();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	std::shared_ptr<GLCube> cube_;
};