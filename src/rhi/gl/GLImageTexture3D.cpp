#include "GLImageTexture3D.hpp"
#include "stb_image.h"

namespace rhi {

GLImageTexture3D::GLImageTexture3D(const std::string& file, int reqChannels)
    : _file(file), _reqChannels(reqChannels) {
}

bool GLImageTexture3D::load() {
    if (!_texture) _texture = std::make_shared<GLTexture3D>();
    int width{0}, height{0}, channels{0};
    unsigned char* pixels = stbi_load(_file.c_str(), &width, &height, &channels, _reqChannels);
    if (!pixels) return false;

    if (channels != 3 && channels != 4) {
        stbi_image_free(pixels);
        return false;
    }

    TextureDataView3D data;
    data.data = pixels;
    data.width = width;
    data.height = height;
    data.depth = 1;
    data.channels = channels;
    const bool ok = _texture->init(data);
    stbi_image_free(pixels);
    return ok;
}

} // namespace rhi
