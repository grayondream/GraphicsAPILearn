#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <memory>
#include <array>


class GLImageTexture2D;
class GLCameraApp : public GLCameraBaseApp {
public:
	virtual ~GLCameraApp();

protected:
	virtual bool initApp() override;

	virtual void beginDrawScene();
	virtual void drawScene(const float dt);

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	float _curTime{};
};