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
class Model;
class GLSaturnApp : public GLCameraBaseApp {

public:
	virtual ~GLSaturnApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void loadModel();

private:
	GLProgram _saturnProgram{};
	GLProgram _rockProgram{};
	std::shared_ptr<Model> _saturn;
	std::shared_ptr<Model> _rock;
	glm::vec3 _saturnPos{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	unsigned int _positionVbo{};
	int _count = 10;
	float _curTime{};
};