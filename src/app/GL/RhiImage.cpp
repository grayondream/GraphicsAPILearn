#include "RhiImage.hpp"
#include "geometry/Image.hpp"
#include "rhi/core/Common.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include <array>
#include <filesystem>

namespace RhiImage {

std::shared_ptr<rhi::ITexture2D> Load2D(rhi::IRenderer* renderer, const std::string& file) {
    Image img(file);
    img.load();
    if (!img.data()) {
        return {};
    }

    auto tex = renderer->createTexture2D();
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGBA8;
    desc.wrapS = rhi::TextureWrap::ClampToEdge;
    desc.wrapT = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::LinearMipLinear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;

    rhi::TextureDataView2D view{img.data(), img.size().width, img.size().height, img.size().channel};
    tex->init(desc, view);
    return tex;
}

std::shared_ptr<rhi::ITexture3D> LoadCube(rhi::IRenderer* renderer, const std::string& dir) {
    const std::array<std::string, 6> faces{
        "right.jpg", "left.jpg", "top.jpg",
        "bottom.jpg", "front.jpg", "back.jpg"
    };

    std::array<Image, 6> imgs{
        Image{}, Image{}, Image{}, Image{}, Image{}, Image{}
    };
    std::array<rhi::TextureDataView2D, 6> views{};
    bool ok = true;
    for (int i = 0; i < 6; ++i) {
        imgs[i] = Image((std::filesystem::path(dir) / faces[i]).string());
        imgs[i].load(false);
        if (!imgs[i].data()) {
            ok = false;
            break;
        }
        views[i] = rhi::TextureDataView2D{imgs[i].data(), imgs[i].size().width,
                                          imgs[i].size().height, imgs[i].size().channel};
    }
    if (!ok) {
        return {};
    }

    auto tex = renderer->createTexture3D();
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGBA8;
    desc.wrapS = rhi::TextureWrap::ClampToEdge;
    desc.wrapT = rhi::TextureWrap::ClampToEdge;
    desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::LinearMipLinear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;

    tex->initCube(desc, views.data());
    return tex;
}

} // namespace RhiImage
