#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <vector>

class GLImageTexture2D;
class GLUniformBufferApp : public GLCameraBaseApp {

public:
	virtual ~GLUniformBufferApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createVertexBuffer();
	unsigned int createUniformBuffer();

private:
	std::vector<GLProgram> _programs{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	float _curTime{};
};