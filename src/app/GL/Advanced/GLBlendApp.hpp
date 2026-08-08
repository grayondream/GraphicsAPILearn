#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLImageTexture2D;
class GLBlendApp : public GLCameraBaseApp {
public:
	virtual ~GLBlendApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createCubeBuffer();
	void createPlaneBuffer();
	void createTexture();
	void compileShader();
	void initGLEnv();

private:
	std::shared_ptr<GLImageTexture2D> _cubeTexture{};
	std::shared_ptr<GLImageTexture2D> _planeTexture{};
	std::shared_ptr<GLImageTexture2D> _grassTexture{};
	std::shared_ptr<GLImageTexture2D> _winTexture{};
	GLProgram _program{};
	unsigned int _cubeVbo[2]{};
	unsigned int _cubeVao{};
	unsigned int _planeVbo[2]{};
	unsigned int _planeVao{};
	float _curTime{};
	glm::vec3 _objectPosition = glm::vec3(0, 0, -4.0f);
	glm::vec3 _objectScale = glm::vec3(20, 1, 20.0f);
	glm::vec3 _winPos = glm::vec3(0.5f, 0.5f, 5);
	int _grassCount = 4;
};