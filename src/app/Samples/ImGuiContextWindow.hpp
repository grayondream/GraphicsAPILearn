#pragma once
#include "app/IImGuiWindow.hpp"

struct GLFWwindow;
// Vulkan 模式下 ImGui 仅初始化核心上下文（无渲染后端）：
// 复用 GL App 的 ImGui 面板代码（ImGui::Begin/End）需要有效 context 与每帧 NewFrame，
// 但 VK 后端不渲染 OpenGL 帧，故不绑定任何渲染后端，画面不含 ImGui 覆盖层。
class ImGuiContextWindow : public IImGuiWindow {
public:
    ~ImGuiContextWindow() override;

public:
    void init(GLFWwindow* win) override;
    void newFrame() override;
    void render() override;
    void destroy() override;

private:
    GLFWwindow* m_window{nullptr};
};
