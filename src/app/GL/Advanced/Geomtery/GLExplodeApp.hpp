#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include <vector>
#include "Geometry/Sphere.hpp"

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