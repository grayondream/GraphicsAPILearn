#pragma once

#include "app/Application.hpp"

class GLApp : public Application {
public:
	GLApp();
	~GLApp();

protected:
	virtual bool initGraphics() override;

protected:
	virtual void clearColor() override;
	virtual void beginDrawScene() override;
	virtual void drawScene(const float dt) override;
	virtual void endDrawScene() override;

protected:
	
};
