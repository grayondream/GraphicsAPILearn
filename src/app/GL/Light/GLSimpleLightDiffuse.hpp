#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"

class GLImageTexture2D;
class GLSimpleLightDiffuse : public GLCameraBaseApp {

public:
	virtual ~GLSimpleLightDiffuse();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void createVertexBuffer();

private:
	Sphere shape{};
	GLProgram _targetProgram{};
	GLProgram _lightProgram{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	float _curTime{};
	
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
};