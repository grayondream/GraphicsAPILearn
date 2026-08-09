#pragma once

#include "app/Application.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class GLApp : public Application {
public:
	GLApp();
	~GLApp();

	std::shared_ptr<rhi::IRenderer> renderer() const { return _renderer; }

protected:
	virtual bool initGraphics() override;

protected:
	virtual void clearColor() override;
	virtual void beginDrawScene() override;
	virtual void drawScene(const float dt) override;
	virtual void endDrawScene() override;

protected:
	std::shared_ptr<rhi::IRenderer> _renderer{};
};
