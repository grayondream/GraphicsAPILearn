#pragma once
#include "GLTexture2D.hpp"
#include <string>
#include <memory>

namespace rhi {

// 从文件加载图片到 GLTexture2D
class GLImageTexture2D {
public:
    GLImageTexture2D(const std::string& file, int reqChannels = 0);
    bool load();
    std::shared_ptr<GLTexture2D> texture() const { return _texture; }
    bool valid() const { return _texture && _texture->valid(); }

private:
    std::string _file{};
    int _reqChannels{0};
    std::shared_ptr<GLTexture2D> _texture{};
};

} // namespace rhi
