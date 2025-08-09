#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"
#include "Geometry/Cube.hpp"

class GLImageTexture2D;
class Model;
class GLLoadModelApp : public GLCameraBaseApp {
public:
	virtual ~GLLoadModelApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void loadModel();
	void drawUI();
	void initProgram(const std::string name, GLProgram& program);

private:
	GLProgram _program{};
	float _curTime{};
	std::shared_ptr<Model> _model{};
};