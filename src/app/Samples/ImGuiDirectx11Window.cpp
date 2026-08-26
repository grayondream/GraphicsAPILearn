#include "ImGuiDirectx11Window.hpp"
#include "base/Log.hpp"
#include <GLFW/glfw3.h>

// GL-only 构建（ENABLE_DX11=OFF）下本文件仍会被 GLOB 编译，但 DX11 头与
// DX11Backend.hpp 不在 include 路径中，且此窗口仅 DX11 模式使用。
#if ENABLE_DX11
#include "rhi/dx11/DX11Backend.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx11.h>
#endif

#include <imgui.h>

ImGuiDirectx11Window::~ImGuiDirectx11Window() {
}

void ImGuiDirectx11Window::setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_renderer = renderer;
}

void ImGuiDirectx11Window::init(GLFWwindow* win) {
    m_window = win;

    if (!m_renderer) {
        LOGE("ImGuiDirectx11Window: renderer not set, overlay disabled");
        return;
    }

#if ENABLE_DX11
    rhi::DX11ImGuiInitInfo initInfo{};
    if (!rhi::GetDX11ImGuiInitInfo(m_renderer, initInfo)) {
        LOGE("ImGuiDirectx11Window: renderer has no DirectX11 imgui info, overlay disabled");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // 平台 backend（D3D11 渲染器无专用 GLFW 变体，官方示例用 InitForOther）
    ImGui_ImplGlfw_InitForOther(win, true);

    // 渲染 backend：字体纹理由 backend 内部自建（DX11 无描述符堆管理负担，
    // 对照 DX12 共享堆槽 0 方案）；device/context 归 DX11Renderer 所有，此处借用
    auto* device = static_cast<ID3D11Device*>(initInfo.device);
    auto* context = static_cast<ID3D11DeviceContext*>(initInfo.context);
    if (!ImGui_ImplDX11_Init(device, context)) {
        LOGE("ImGui_ImplDX11_Init failed, overlay disabled");
        return;
    }

    m_ready = true;
#else
    (void)win;
    LOGE("ImGuiDirectx11Window: DirectX11 backend disabled in build, overlay disabled");
#endif
}

void ImGuiDirectx11Window::newFrame() {
#if ENABLE_DX11
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiDirectx11Window::render() {
#if ENABLE_DX11
    ImGui::Render();
    if (m_ready && m_renderer)
        m_renderer->renderImGuiDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiDirectx11Window::destroy() {
#if ENABLE_DX11
    if (m_ready) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
#endif
    m_renderer.reset();
}
