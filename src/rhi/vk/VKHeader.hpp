#pragma once
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

namespace rhi {

inline uint32_t findMemoryType(const vk::PhysicalDeviceMemoryProperties& props,
                               uint32_t typeBits, vk::MemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }
    return UINT32_MAX;
}

} // namespace rhi
