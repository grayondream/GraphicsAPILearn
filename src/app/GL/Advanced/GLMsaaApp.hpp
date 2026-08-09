#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "native/GL/GLProgram.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"

class GLImageTexture2D;
class GLMsaaApp : public GLCameraBaseApp {
public:
	virtual ~GLMsaaApp();
	unsigned int getSampleCount() const override { return 4; }
	
protected:
	virtual void beginDrawScene() override;
	virtual bool initApp() override;
	virtual void drawScene(const float dt);
	
private:
	void createVertexBuffer();
	void createScreenBuffer();
	void createFrameBuffer();
	void createPostFrameBuffer();
	void drawGLMssa();
	void drawFrameBufferMssa();

private:
	std::shared_ptr<GLImageTexture2D> _texture{};
	GLProgram _program{};
	GLProgram _postProgram{};
	unsigned int _vbo[2]{};
	unsigned int _vao{};
	unsigned int _screenVao{};
	unsigned int _screenVbo[2];
	unsigned int _screenEbo{};
	unsigned int _screenFrameBuffer{};
	unsigned int _screenRbo{};
	unsigned int _postFrameBuffer{};
	unsigned int _postTexture{};
	float _curTime{};
	bool _enableMsaa{ false };
	bool _enableFrameBufferMssa{ false };
};