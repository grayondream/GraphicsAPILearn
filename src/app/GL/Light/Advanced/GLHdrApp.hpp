#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class GLImageTexture2D;
class GLHdrApp : public GLCameraBaseApp {
public:
	virtual ~GLHdrApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void createTextures();
	void render2FrameBuffer();
	void renderHdr();

private:
	GLProgram _objProgram;
	GLProgram _hdrProgram;
	float _curTime{};
	unsigned int _vao{};
	unsigned int _vbo{};
	unsigned int _screenVao{};
	unsigned int _screenVbo{};
	unsigned int _colorBuffer{};
	unsigned int _hdrFBO{};

	bool _enableHdr = true;
	float _exposure = 0.5f;
	std::shared_ptr<GLImageTexture2D> _brick{};
	std::shared_ptr<GLImageTexture2D> _brickNormal{};
	std::shared_ptr<GLImageTexture2D> _brickDisp{};
};