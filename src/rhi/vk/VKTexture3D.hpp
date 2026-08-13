#pragma once
#include "VKHeader.hpp"
#include "rhi/core/ITexture3D.hpp"

namespace rhi {

class VKTexture3D : public ITexture3D {
public:
    explicit VKTexture3D(vk::raii::Device& device) : _dev(device) {}

    bool init(const TextureDataView3D&) override { return false; }
    bool initCube(const TextureDesc&, const TextureDataView2D*) override { return false; }
    bool createEmpty(const TextureDesc&, int, int) override { return false; }
    void bind(unsigned int) override {}
    void* handle() override { return nullptr; }
    bool valid() const override { return false; }
    void release() override {}

private:
    vk::raii::Device& _dev;
};

} // namespace rhi