#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>

class GLImageTexture2D;
class GLSimpleGemoteryApp : public GLCameraBaseApp {
public:
	virtual ~GLSimpleGemoteryApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createVertexBuffer();

private:
	GLProgram _program;
	unsigned int _vbo{};
	unsigned int _vao{};
	float _curTime{};
};