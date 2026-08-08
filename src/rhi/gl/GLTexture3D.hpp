#pragma once
#include "rhi/core/ITexture3D.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLTexture3D : public ITexture3D {
public:
    ~GLTexture3D();
    bool init(const TextureDataView3D& data) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _id != 0; }
    void release() override;

private:
    GLuint _id{0};
};

} // namespace rhi
