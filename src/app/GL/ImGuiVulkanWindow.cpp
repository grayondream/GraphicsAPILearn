#include "ImGuiVulkanWindow.hpp"
#include "base/Log.hpp"
#include <GLFW/glfw3.h>

// GL-only 构建（ENABLE_VULKAN=OFF）下本文件仍会被 GLOB 编译，但 Vulkan 头与
// VKBackend.hpp（含 vulkan.hpp）不在 include 路径中，且此窗口仅 Vulkan 模式使用。
#if ENABLE_VULKAN
#include "rhi/vk/VKBackend.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

#include <imgui.h>

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

#if ENABLE_VULKAN
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
    vkInfo.MinImageCount = 2;   // backend 断言 MinImageCount>=2；VKSwapchain 强制 imageCount>=3
    vkInfo.ImageCount = initInfo.imageCount;
    vkInfo.MinAllocationSize = initInfo.minAllocationSize;
    vkInfo.PipelineInfoMain.RenderPass =
        static_cast<VkRenderPass>(initInfo.renderPass);

    if (!ImGui_ImplVulkan_Init(&vkInfo)) {
        LOGE("ImGui_ImplVulkan_Init failed, overlay disabled");
        return;
    }

    m_ready = true;
#else
    (void)win;
    LOGE("ImGuiVulkanWindow: Vulkan backend disabled in build, overlay disabled");
#endif
}

void ImGuiVulkanWindow::newFrame() {
#if ENABLE_VULKAN
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiVulkanWindow::render() {
#if ENABLE_VULKAN
    ImGui::Render();
    if (m_ready && m_renderer)
        m_renderer->renderImGuiDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiVulkanWindow::destroy() {
#if ENABLE_VULKAN
    if (m_ready) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_ready = false;
    }
#endif
    m_renderer.reset();
}
