#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>
#include "geometry/Sphere.hpp"

class GLImageTexture2D;
class GLExplodeApp : public GLCameraBaseApp {
public:
	virtual ~GLExplodeApp();

	protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void createVertexBuffer();

private:
	Sphere shape{};
	GLProgram _program;
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	float _curTime{};
};