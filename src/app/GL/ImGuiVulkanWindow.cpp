#include "ImGuiVulkanWindow.hpp"
#include "base/Log.hpp"
#include "rhi/vk/VKBackend.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

ImGuiVulkanWindow::~ImGuiVulkanWindow() {
}

void ImGuiVulkanWindow::setRenderer(const std::shared_ptr<rhi::IRenderer>& renderer) {
    m_renderer = renderer;
}

void ImGuiVulkanWindow::init(GLFWwindow* win) {
    m_window = win;

    if (!m_renderer) {
        LOGE("ImGuiVulkanWindow: renderer not set, overlay disabled");
        return;
    }

    rhi::VKImGuiInitInfo initInfo{};
    if (!m_renderer->imguiInitInfo(initInfo)) {
        LOGE("ImGuiVulkanWindow: renderer has no Vulkan imgui info, overlay disabled");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // 平台 backend
    ImGui_ImplGlfw_InitForVulkan(win, true);

    // 渲染 backend：DescriptorPoolSize>0 让 backend 自建内部 pool/pipeline，
    // 不占用 3D 的 _dsPool/VKRenderer 描述符（_uboDs）。
    ImGui_ImplVulkan_InitInfo vkInfo{};
    vkInfo.ApiVersion = VK_API_VERSION_1_0;
    vkInfo.Instance = static_cast<VkInstance>(initInfo.instance);
    vkInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(initInfo.physDevice);
    vkInfo.Device = static_cast<VkDevice>(initInfo.device);
    vkInfo.QueueFamily = initInfo.graphicsFamily;
    vkInfo.Queue = static_cast<VkQueue>(initInfo.graphicsQueue);
    vkInfo.DescriptorPoolSize = 256;
    vkInfo.MinImageCount = initInfo.imageCount > 2 ? 2 : 1;
    vkInfo.ImageCount = initInfo.imageCount;
    vkInfo.MinAllocationSize = initInfo.minAllocationSize;
    vkInfo.PipelineInfoMain.RenderPass =
        static_cast<VkRenderPass>(initInfo.renderPass);

    if (!ImGui_ImplVulkan_Init(&vkInfo)) {
        LOGE("ImGui_ImplVulkan_Init failed, overlay disabled");
        return;
    }

    m_ready = true;
}

void ImGuiVulkanWindow::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiVulkanWindow::render() {
    ImGui::Render();
    if (m_ready && m_renderer)
        m_renderer->renderImGuiDrawData(ImGui::GetDrawData());
}

void ImGuiVulkanWindow::destroy() {
    if (m_ready) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
    m_renderer.reset();
}
