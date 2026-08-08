#pragma once
#include "rhi/core/ITexture2D.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLTexture2D : public ITexture2D {
public:
    ~GLTexture2D();
    bool init(const TextureDataView2D& data) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _id != 0; }
    void release() override;

private:
    GLuint _id{0};
};

} // namespace rhi
