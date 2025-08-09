#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLTemplateTestApp : public GLCameraBaseApp {

public:
	virtual ~GLTemplateTestApp();

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
	GLProgram _borderProgram{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	float _curTime{};
};