#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLImageTexture2D;
class GLFrameBufferApp : public GLCameraBaseApp {
public:
	virtual ~GLFrameBufferApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createCubeBuffer();
	void createPlaneBuffer();
	void createScreenBuffer();
	void createFrameBuffer();
	void compileShader();
	void loadTexture();
	void initGLEnv();
	void drawPlane();
	void drawCube();
	GLProgram compileShader(const std::string& name);

private:
	std::shared_ptr<GLImageTexture2D> _cubeTexture{};
	std::shared_ptr<GLImageTexture2D> _planeTexture{};
	GLProgram _contentProgram{};
	GLProgram _screenProgram{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	unsigned int _screenVao{};
	unsigned int _screenVbo[2];
	unsigned int _screenEbo{};
	unsigned int _screenFrameBuffer{};
	unsigned int _screenTextureId{};
	unsigned int _screenRbo{};
	float _curTime{};
	int _selectEffectType{ 0 };
};