#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLImageTexture2D;
class GLDepthTestApp : public GLCameraBaseApp {

public:
	virtual ~GLDepthTestApp();

protected:
	virtual bool initApp() override;
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);

private:
	void createCubeBuffer();
	void createPlaneBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _cubeTexture{};
	std::shared_ptr<GLImageTexture2D> _planeTexture{};
	GLProgram _program{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	float _curTime{};
};