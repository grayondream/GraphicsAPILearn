#include "RhiGeometry.hpp"
#include "geometry/Shape.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/Common.hpp"

namespace RhiGeometry {

Geometry Create(rhi::IRenderer* renderer, Shape& shape,
                bool useUv, bool useNormal, bool useIndex,
                const Layout& layout) {
    using namespace rhi;
    Geometry g;

    g.vertexCount = static_cast<uint32_t>(shape.size());
    const float stride32 = 32.0f;

    // binding 0：交错 pos+color
    auto vb = renderer->createBuffer();
    vb->init(shape.toGL().data(), shape.byteSize(), BufferType::Vertex);
    g.vertexBuffer = vb;
    g.layout.elements.push_back(VertexElement{VertexElement::Float4, 0, 0,
                                              VertexInputRate::PerVertex, 0, static_cast<int>(stride32)});
    g.layout.elements.push_back(VertexElement{VertexElement::Float4, 1, 0,
                                              VertexInputRate::PerVertex, 16, static_cast<int>(stride32)});

    if (useUv && shape.uvSize() > 0) {
        auto ub = renderer->createBuffer();
        ub->init(shape.uv(), shape.uvSize(), BufferType::Vertex);
        g.uvBuffer = ub;
        g.layout.elements.push_back(VertexElement{VertexElement::Float2, layout.uvLocation, 1,
                                                  VertexInputRate::PerVertex, 0, 8});
    }
    if (useNormal && shape.normalSize() > 0) {
        auto nb = renderer->createBuffer();
        nb->init(shape.normal(), shape.normalSize(), BufferType::Vertex);
        g.normalBuffer = nb;
        g.layout.elements.push_back(VertexElement{VertexElement::Float4, layout.normalLocation, 2,
                                                  VertexInputRate::PerVertex, 0, 16});
    }
    if (useIndex && shape.idxSize() > 0) {
        auto ib = renderer->createBuffer();
        ib->init(shape.idx(), shape.idxByteSize(), BufferType::Index);
        g.indexBuffer = ib;
        g.indexCount = static_cast<uint32_t>(shape.idxSize());
    }
    return g;
}

Geometry CreateFromArray(rhi::IRenderer* renderer, const float* data, size_t byteSize,
                         uint32_t vertexCount, const rhi::VertexLayout& layout) {
    using namespace rhi;
    Geometry g;
    g.vertexCount = vertexCount;
    auto vb = renderer->createBuffer();
    vb->init(data, byteSize, BufferType::Vertex);
    g.vertexBuffer = vb;
    g.layout = layout;
    return g;
}

} // namespace RhiGeometry
