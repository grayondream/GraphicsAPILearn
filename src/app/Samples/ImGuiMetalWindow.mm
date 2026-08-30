#include "ImGuiMetalWindow.hpp"
#include "rhi/mtl/MetalBackend.hpp"
#include "base/Log.hpp"
#include <imgui.h>
#include "imgui_impl_metal.h"
#include "imgui_impl_glfw.h"
#include "GLFW/glfw3.h"

#import <Metal/Metal.h>

ImGuiMetalWindow::~ImGuiMetalWindow() { destroy(); }

void ImGuiMetalWindow::setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_renderer = renderer;
}

void ImGuiMetalWindow::init(GLFWwindow* win) {
    m_window = win;
    rhi::mtl::MetalImGuiInitInfo info{};
    if (!rhi::mtl::GetMetalImGuiInitInfo(m_renderer, info)) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    auto* device = (__bridge id<MTLDevice>)info.device;
    ImGui_ImplMetal_Init(device);
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    m_ready = true;
}

void ImGuiMetalWindow::newFrame() {
    if (!m_ready) return;
    rhi::mtl::MetalImGuiInitInfo info{};
    if (!rhi::mtl::GetMetalImGuiInitInfo(m_renderer, info)) return;

    auto* rpd = (__bridge MTLRenderPassDescriptor*)info.renderPassDescriptor;
    if (rpd) {
        ImGui_ImplMetal_NewFrame(rpd);
    }
    ImGui_ImplGlfw_NewFrame();
    // Metal 交换链 drawable 使用点尺寸(1x)，与 GLFW 返回的 Retina 内容缩放(2x)不一致，
    // 若沿用 2x 的 framebufferScale，ImGui 的 scissor/clip 矩形会超出 1x 渲染目标而
    // 触发 Metal 校验失败。这里强制为 1x 以匹配 1x drawable。
    ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui::NewFrame();
}

void ImGuiMetalWindow::render() {
    if (!m_ready) return;
    rhi::mtl::MetalImGuiInitInfo info{};
    if (!rhi::mtl::GetMetalImGuiInitInfo(m_renderer, info)) return;

    ImGui::Render();
    auto* cmdBuf = (__bridge id<MTLCommandBuffer>)info.commandBuffer;
    auto* encoder = (__bridge id<MTLRenderCommandEncoder>)info.renderEncoder;
    ImDrawData* dd = ImGui::GetDrawData();
    if (cmdBuf && encoder && dd) {
        ImGui_ImplMetal_RenderDrawData(dd, cmdBuf, encoder);
    }
}

void ImGuiMetalWindow::destroy() {
    if (m_ready) {
        ImGui_ImplGlfw_Shutdown();
        ImGui_ImplMetal_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
}
