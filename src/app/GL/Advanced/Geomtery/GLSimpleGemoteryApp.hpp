#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
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