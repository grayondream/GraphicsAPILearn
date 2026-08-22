#pragma once
#include "app/IImGuiWindow.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

struct GLFWwindow;

// DirectX12 模式的 ImGui 窗口：GLFW 平台 backend + DX12 渲染 backend。
// 复用 DXRenderer 暴露的只读句柄与 present 前录制钩子，与 GL/VK 端行为一致。
class ImGuiDirectx12Window : public IImGuiWindow{
public:
    ~ImGuiDirectx12Window() override;

public:
    virtual void init(GLFWwindow* win) override;
    virtual void newFrame() override;
    virtual void render() override;
    virtual void destroy() override;

public:
    // ImGuiDirectx12Window 的 init 需要 DXRenderer 句柄，但 IImGuiWindow::init 签名不含
    // renderer，故单独注入（AppHost DX12 分支在 init 前调用）。
    void setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer);

private:
    GLFWwindow* m_window{nullptr};
    std::shared_ptr<rhi::IRenderer> m_renderer{};
    bool m_ready{false};
};
