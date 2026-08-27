#pragma once
#include "app/IImGuiWindow.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

struct GLFWwindow;

class ImGuiMetalWindow : public IImGuiWindow {
public:
    ~ImGuiMetalWindow() override;
    void init(GLFWwindow* win) override;
    void newFrame() override;
    void render() override;
    void destroy() override;
    void setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer);

private:
    GLFWwindow* m_window{nullptr};
    std::shared_ptr<rhi::IRenderer> m_renderer{};
    bool m_ready{false};
};
