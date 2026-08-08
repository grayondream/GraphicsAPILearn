#pragma once
#include "GLTexture3D.hpp"
#include <string>
#include <memory>

namespace rhi {

// 从文件加载体积/3D 纹理到 GLTexture3D
class GLImageTexture3D {
public:
    GLImageTexture3D(const std::string& file, int reqChannels = 0);
    bool load();
    std::shared_ptr<GLTexture3D> texture() const { return _texture; }
    bool valid() const { return _texture && _texture->valid(); }

private:
    std::string _file{};
    int _reqChannels{0};
    std::shared_ptr<GLTexture3D> _texture{};
};

} // namespace rhi
