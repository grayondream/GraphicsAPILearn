#if ENABLE_DX12
#include "ImGuiDirectx12Window.hpp"
#include "base/Log.hpp"
#include <GLFW/glfw3.h>

// GL-only 构建（ENABLE_DX12=OFF）下本文件仍会被 GLOB 编译，但 DX12 头与
// DXBackend.hpp 不在 include 路径中，且此窗口仅 DX12 模式使用。
#if ENABLE_DX12
#include "rhi/dx12/DXBackend.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_dx12.h>
#endif

#include <imgui.h>

ImGuiDirectx12Window::~ImGuiDirectx12Window() {
}

void ImGuiDirectx12Window::setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_renderer = renderer;
}

void ImGuiDirectx12Window::init(GLFWwindow* win) {
    m_window = win;

    if (!m_renderer) {
        LOGE("ImGuiDirectx12Window: renderer not set, overlay disabled");
        return;
    }

#if ENABLE_DX12
    rhi::DXImGuiInitInfo initInfo{};
    if (!rhi::GetDXImGuiInitInfo(m_renderer, initInfo)) {
        LOGE("ImGuiDirectx12Window: renderer has no DirectX12 imgui info, overlay disabled");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // 平台 backend（DX12 渲染器无专用 GLFW 变体，官方示例用 InitForOther）
    ImGui_ImplGlfw_InitForOther(win, true);

    // 渲染 backend：字体纹理 SRV 占共享 CBV_SRV_UAV 堆槽 0（DXRenderer Task 7
    // 预留，场景 bindTexture 从槽 1 起）；RTV 格式与 DXSwapchain 固定创建值一致；
    // frames in flight 与交换链 BufferCount=2 相同。
    auto* device = static_cast<ID3D12Device*>(initInfo.device);
    auto* srvHeap = static_cast<ID3D12DescriptorHeap*>(initInfo.srvHeap);
    D3D12_CPU_DESCRIPTOR_HANDLE fontSrvCpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE fontSrvGpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
    if (!ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap,
                             fontSrvCpu, fontSrvGpu)) {
        LOGE("ImGui_ImplDX12_Init failed, overlay disabled");
        return;
    }

    m_ready = true;
#else
    (void)win;
    LOGE("ImGuiDirectx12Window: DirectX12 backend disabled in build, overlay disabled");
#endif
}

void ImGuiDirectx12Window::newFrame() {
#if ENABLE_DX12
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiDirectx12Window::render() {
#if ENABLE_DX12
    ImGui::Render();
    if (m_ready && m_renderer)
        m_renderer->renderImGuiDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiDirectx12Window::destroy() {
#if ENABLE_DX12
    if (m_ready) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
#endif
    m_renderer.reset();
}

#endif
