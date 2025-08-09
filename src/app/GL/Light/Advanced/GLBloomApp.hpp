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
class GLPlane;
class GLCube;
class GLBloomApp : public GLCameraBaseApp {
public:
	virtual ~GLBloomApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void initShapes();
	void createTextures();
	void extractBrightPart();
	
private:
	GLProgram _bloomProgram;
	float _curTime{};	
	bool _enableHdr = true;
	float _exposure = 0.5f;
	std::shared_ptr<GLImageTexture2D> wood_{};
	std::shared_ptr<GLImageTexture2D> brick_{};
	std::shared_ptr<GLCube> cube_{};
	std::shared_ptr<GLPlane> plane_{};
};