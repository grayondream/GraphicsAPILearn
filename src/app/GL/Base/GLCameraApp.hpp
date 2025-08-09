#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
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