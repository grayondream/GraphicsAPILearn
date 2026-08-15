#pragma once
#include "VKHeader.hpp"
#include "rhi/core/IBuffer.hpp"

namespace rhi {

class VKBuffer : public IBuffer {
public:
    class Notifier {
    public:
        virtual ~Notifier() = default;
        virtual void onUniformCreated(VKBuffer* buffer, size_t offset, size_t size) = 0;
        virtual void onUniformUpdated(VKBuffer* buffer, uint32_t slot, size_t offset, size_t size) = 0;
    };

    explicit VKBuffer(vk::raii::Device& device, vk::raii::PhysicalDevice& phys,
                      vk::raii::Queue& queue, uint32_t graphicsFamily);
    ~VKBuffer() override = default;

    bool init(const void* data, size_t size, BufferType type) override;
    bool update(const void* data, size_t size, size_t offset = 0) override;
    bool bindRange(uint32_t binding, size_t offset, size_t size) override;
    bool bind() override;
    void* handle() override;

    vk::Buffer raw() const { return *_buffer; }
    VkDeviceSize size() const { return _size; }
    BufferType type() const { return _type; }
    void setNotifier(Notifier* notifier) { _notifier = notifier; }

private:
    bool uploadStaging(const void* data, size_t size, size_t offset);

    vk::raii::Device& _dev;
    vk::raii::PhysicalDevice& _phys;
    vk::raii::Queue& _queue;
    uint32_t _graphicsFamily{0};
    Notifier* _notifier{nullptr};
    BufferType _type{BufferType::Vertex};
    VkDeviceSize _size{0};
    vk::MemoryPropertyFlags _memProps{};
    vk::raii::Buffer _buffer{nullptr};
    vk::raii::DeviceMemory _memory{nullptr};
    vk::raii::CommandPool _stagingPool{nullptr};
    static constexpr size_t kRingSlots = 32;
    size_t _slotSize{0};        // 单槽字节数（uniform 时 = 原始 size）
    uint32_t _ringHead{0};      // 累计更新计数，取模 kRingSlots 得当前槽
    VkDeviceSize _submittedBase{0};  // descriptor 当前绑定的槽 base offset
};

} // namespace rhi