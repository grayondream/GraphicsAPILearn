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
class GLNormalMapApp : public GLCameraBaseApp {
public:
	virtual ~GLNormalMapApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void createTextures();

private:
	GLProgram _program;
	float _curTime{};
	unsigned int _vao{};
	unsigned int _vbo{};
	bool _enableNormalMap{};
	std::shared_ptr<GLImageTexture2D> _brick{};
	std::shared_ptr<GLImageTexture2D> _brickNormal{};
};