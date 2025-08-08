#pragma once

#include "App/Application.hpp"

class GLApp : public Application {
public:
	GLApp();
	~GLApp();

protected:
	virtual bool initGraphics() override;

protected:
	virtual void clearColor();
	virtual void beginDrawScene();
	virtual void drawScene(const float dt);
	virtual void endDrawScene();

protected:
	
};
