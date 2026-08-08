#include "GLImageTexture2D.hpp"
#include "stb_image.h"

namespace rhi {

GLImageTexture2D::GLImageTexture2D(const std::string& file, int reqChannels)
    : _file(file), _reqChannels(reqChannels) {
}

bool GLImageTexture2D::load() {
    if (!_texture) _texture = std::make_shared<GLTexture2D>();
    int width{0}, height{0}, channels{0};
    unsigned char* pixels = stbi_load(_file.c_str(), &width, &height, &channels, _reqChannels);
    if (!pixels) return false;

    if (channels != 3 && channels != 4) {
        stbi_image_free(pixels);
        return false;
    }

    TextureDataView2D data;
    data.data = pixels;
    data.width = width;
    data.height = height;
    data.channels = channels;
    const bool ok = _texture->init(data);
    stbi_image_free(pixels);
    return ok;
}

} // namespace rhi
