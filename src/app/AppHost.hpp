#pragma once
#include "IApplication.hpp"
#include "app/AppType.hpp"
#include <memory>

class GLFWWindow;
class IImGuiWindow;
class Sample;
namespace rhi { class IRenderer; }

class AppHost : public IApplication {
public:
    AppHost();
    ~AppHost();
    bool init(const GLFWWindowProperties& properties) override;
    int run() override;
    void exit() override;
    unsigned int getSampleCount() const override;

    void setSample(AppType type);
    void setBackend(GraphicsType type);
    AppType currentSample() const { return _sampleType; }
    GraphicsType currentBackend() const { return _backend; }

private:
    void hookWindowCallbacks();
    void forwardKey(int key, int scancode, int action, int mods);
    void forwardMouseMove(double x, double y);
    void forwardMouseScroll(double xoffset, double yoffset);
    void forwardMouseButton(int button, int action, int mods);
    void forwardWinResize(int width, int height);
    bool rebuildBackend(const GLFWWindowProperties& props);
    bool reloadSample();
    void applyPendingChanges();
    void destroyBackendResources();
    void renderTotalBar();
    float aspect() const;

    std::unique_ptr<GLFWWindow> m_window{};
    std::unique_ptr<IImGuiWindow> m_imguiWindow{};
    std::shared_ptr<rhi::IRenderer> _renderer{};
    std::shared_ptr<Sample> _sample{};
    AppType _sampleType{ AppType::Base };
    GraphicsType _backend{ GraphicsType::GL };
    bool _pendingSample{ false };
    bool _pendingBackend{ false };
    bool _exitRequested{ false };
};
