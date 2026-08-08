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