#if ENABLE_DX11
#pragma once
#include "app/IImGuiWindow.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

struct GLFWwindow;

// DirectX11 模式的 ImGui 窗口：GLFW 平台 backend + DX11 渲染 backend。
// 结构对照 ImGuiDirectx12Window：init 经 GetDX11ImGuiInitInfo 桥取 device/context，
// 渲染经 IRenderer::renderImGuiDrawData 钩子（DX11Renderer 负责回绑窗口 OM）。
class ImGuiDirectx11Window : public IImGuiWindow{
public:
    ~ImGuiDirectx11Window() override;

public:
    virtual void init(GLFWwindow* win) override;
    virtual void newFrame() override;
    virtual void render() override;
    virtual void destroy() override;

public:
    // init 需要 renderer 句柄，但 IImGuiWindow::init 签名不含 renderer，
    // 故单独注入（AppHost DX11 分支在 init 前调用，同 DX12 模式）。
    void setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer);

private:
    GLFWwindow* m_window{nullptr};
    std::shared_ptr<rhi::IRenderer> m_renderer{};
    bool m_ready{false};
};

#endif
