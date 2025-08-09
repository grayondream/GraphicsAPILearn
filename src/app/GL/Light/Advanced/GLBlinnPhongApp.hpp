#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"
#include "Geometry/Sphere.hpp"

class GLImageTexture2D;
class GLBlinnPhongApp : public GLCameraBaseApp {
public:
	virtual ~GLBlinnPhongApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void createVertexBuffer();
	void createPlaneBuffer();
	
private:
	Sphere shape{};
	GLProgram _targetProgram{};
	GLProgram _lightProgram{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	unsigned int _planeVbo[3]{};
	unsigned int _planeVao{};
	bool _enableBlinnPhong{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};