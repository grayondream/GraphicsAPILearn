#pragma once
#include "rhi/core/ITexture3D.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLTexture3D : public ITexture3D {
public:
    ~GLTexture3D();
    bool init(const TextureDataView3D& data) override;
    bool initCube(const TextureDesc& desc, const TextureDataView2D* faces) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _id != 0; }
    void release() override;
    const TextureDesc& desc() const { return _desc; }

private:
    GLuint _id{0};
    GLenum _target{GL_TEXTURE_3D};
    TextureDesc _desc{};
};

} // namespace rhi
