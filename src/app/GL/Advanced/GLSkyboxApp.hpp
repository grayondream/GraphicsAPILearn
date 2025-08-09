
#pragma once
#include "App/GL/Base/GLCameraBaseApp.hpp"
#include "Native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "Geometry/Camera.hpp"
#include "Geometry/Vertex.hpp"

class GLImageTexture2D;
class GLImageTexture3D;
class GLSkyboxApp : public GLCameraBaseApp {
public:
	virtual ~GLSkyboxApp();

	protected:
	virtual bool initApp() override;
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	
private:
	void createVertexBuffer();
	void drawCube();
	void drawSkybox();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	std::shared_ptr<GLImageTexture3D> _skyBoxTexture{};
	GLProgram _program{};
	GLProgram _skyboxProgram{};
	unsigned int _vbo[3]{};
	unsigned int _vao{};
	unsigned int _skyVao{};
	unsigned int _skyVbo{};
	float _curTime{};
	bool _enableReflect{};
	bool _enableRefraction{};
};