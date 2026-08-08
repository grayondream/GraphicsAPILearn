#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLImageTexture2D;
class GLAdvancedGLSLApp : public GLCameraBaseApp {
public:
	virtual ~GLAdvancedGLSLApp();

protected:
	virtual bool initApp();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);

private:
	void createVertexBuffer();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	float _curTime{};
    bool _enablePointSize;
    bool _enableFragCoord;
    bool _enableVertexId;
    bool _enableFrontFaceCulling;
};