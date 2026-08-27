#include "ImGuiMetalWindow.hpp"
#include "rhi/mtl/MetalBackend.hpp"
#include <imgui.h>
#include "imgui_impl_metal.h"
#include "GLFW/glfw3.h"

ImGuiMetalWindow::~ImGuiMetalWindow() { destroy(); }

void ImGuiMetalWindow::setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_renderer = renderer;
}

void ImGuiMetalWindow::init(GLFWwindow* win) {
    m_window = win;
    rhi::MetalImGuiInitInfo info{};
    if (!rhi::GetMetalImGuiInitInfo(m_renderer, info)) return;

    ImGui::CreateContext();
    auto* device = (__bridge id<MTLDevice>)info.device;
    ImGui_ImplMetal_Init(device);
    m_ready = true;
}

void ImGuiMetalWindow::newFrame() {
    if (!m_ready) return;
    ImGui_ImplMetal_NewFrame();
    ImGui_ImplGlfw_NewFrame(m_window);
    ImGui::NewFrame();
}

void ImGuiMetalWindow::render() {
    if (!m_ready) return;
    ImGui::Render();
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiMetalWindow::destroy() {
    if (m_ready) {
        ImGui_ImplMetal_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
}
