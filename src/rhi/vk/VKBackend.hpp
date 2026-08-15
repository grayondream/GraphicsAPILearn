#pragma once
#include "rhi/core/IRenderer.hpp"
#include "rhi/vk/VKHeader.hpp"

namespace rhi {

class VKTexture2D;
class VKTexture3D;

// ImGui_ImplVulkan_Init 初始化所需原生句柄（App 层 ImGuiVulkanWindow 用它填充
// ImGui_ImplVulkan_InitInfo）。由 VKRenderer::imguiInitInfo 填充。
struct VKImGuiInitInfo {
    vk::Instance instance{};
    vk::PhysicalDevice physDevice{};
    vk::Device device{};
    uint32_t graphicsFamily{0};
    vk::Queue graphicsQueue{};
    vk::DescriptorPool dsPool{};
    vk::RenderPass renderPass{};
    uint32_t imageCount{0};
    vk::DeviceSize minAllocationSize{1u << 20};
};

std::shared_ptr<IRenderer> createVKRenderer();

} // namespace rhi
