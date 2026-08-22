#include "ImGuiContextWindow.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>

ImGuiContextWindow::~ImGuiContextWindow() {
}

void ImGuiContextWindow::init(GLFWwindow* win) {
    m_window = win;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    // 无渲染后端：预构建字体纹理，避免无后端断言（TexIsBuilt）。
    io.Fonts->Build();
}

void ImGuiContextWindow::newFrame() {
    ImGuiIO& io = ImGui::GetIO();
    if (m_window) {
        int w = 0, h = 0;
        glfwGetFramebufferSize(m_window, &w, &h);
        if (w > 0 && h > 0)
            io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    }
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
        io.DisplaySize = ImVec2(1280.0f, 720.0f);
    static double prevTime = 0.0;
    double now = glfwGetTime();
    io.DeltaTime = (prevTime > 0.0) ? static_cast<float>(now - prevTime)
                                    : static_cast<float>(1.0 / 60.0);
    prevTime = now;
    ImGui::NewFrame();
}

void ImGuiContextWindow::render() {
    ImGui::Render();
}

void ImGuiContextWindow::destroy() {
    ImGui::DestroyContext();
}
