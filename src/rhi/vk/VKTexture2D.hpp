#pragma once
#include "VKHeader.hpp"
#include "rhi/core/ITexture2D.hpp"

namespace rhi {

class VKTexture2D : public ITexture2D {
public:
    explicit VKTexture2D(vk::raii::Device& device) : _dev(device) {}

    bool init(const TextureDataView2D&) override { return false; }
    bool init(const TextureDesc&, const TextureDataView2D&) override { return false; }
    bool createEmpty(const TextureDesc&, int, int) override { return false; }
    void bind(unsigned int) override {}
    void* handle() override { return nullptr; }
    bool valid() const override { return false; }
    void release() override {}

private:
    vk::raii::Device& _dev;
};

} // namespace rhi