#pragma once
#include "app/IImGuiWindow.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

struct GLFWwindow;

// Vulkan 模式的 ImGui 窗口：GLFW 平台 backend + Vulkan 渲染 backend。
// 复用 VKRenderer 暴露的只读句柄与活跃 render pass 内绘制钩子，与 GL 端行为一致。
class ImGuiVulkanWindow : public IImGuiWindow{
public:
    ~ImGuiVulkanWindow() override;

public:
    virtual void init(GLFWwindow* win) override;
    virtual void newFrame() override;
    virtual void render() override;
    virtual void destroy() override;

public:
    // ImGuiVulkanWindow 的 init 需要 VKRenderer 句柄，但 IImGuiWindow::init 签名不含
    // renderer，故单独注入（GLApp vulkan 分支在 init 前调用）。
    void setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer);

private:
    GLFWwindow* m_window{nullptr};
    std::shared_ptr<rhi::IRenderer> m_renderer{};
    bool m_ready{false};
};
