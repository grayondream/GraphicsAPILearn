#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"

class GLImageTexture2D;
class GLShadowMapApp : public GLCameraBaseApp {
public:
	virtual ~GLShadowMapApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void createShadowDepthBuffer();
	void createVertexBuffer();
	void createPlaneBuffer();
	void createScreenBuffer();
	void compileShader();
	void reanderFraemBuffer();
	void renderScene2FrameBuffer();

private:
	Sphere shape{};
	GLProgram _shadowProgram{};
	GLProgram _depthProgram{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _ebo{};
	unsigned int _planeVbo[3]{};
	unsigned int _planeVao{};
	unsigned int _shadowDepthMapFbo{};
	unsigned int _shadowDepthMap{};
	unsigned int _screenVao{};
	unsigned int _screenVbo[2];
	unsigned int _screenEbo{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<GLImageTexture2D> _texture{};
};