#pragma once
#include "rhi/core/IRenderer.hpp"
#include <memory>

class Shape;

namespace RhiGeometry {
// 上传一个 Shape 到 RHI 缓冲，并生成对应的 VertexLayout。
// 顶点数据用 shape.toGL()（GL 坐标约定，RHI 层不做坐标转换）。
struct Geometry {
    std::shared_ptr<rhi::IBuffer> vertexBuffer{};  // binding 0：交错 pos(vec4)+color(vec4)，stride 32
    std::shared_ptr<rhi::IBuffer> uvBuffer{};      // binding 1：uv(vec2)，stride 8（useUv 时）
    std::shared_ptr<rhi::IBuffer> normalBuffer{};  // binding 2：normal(vec4)，stride 16（useNormal 时）
    std::shared_ptr<rhi::IBuffer> indexBuffer{};   // useIndex 时
    rhi::VertexLayout layout{};
    uint32_t vertexCount{0};
    uint32_t indexCount{0};
};

// 可选布局参数：指定 uv / normal 的 location（GL semantic）。
// 默认 uv=2、normal=3（与已迁移模板一致）；SimpleLight 系列用 uv=3、normal=2。
struct Layout {
    int uvLocation{2};
    int normalLocation{3};
};

Geometry Create(rhi::IRenderer* renderer, Shape& shape,
                bool useUv, bool useNormal, bool useIndex,
                const Layout& layout = {});
}
