#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace rhi {

enum class PrimitiveType : uint8_t { TriangleList, TriangleStrip, Lines };

struct Viewport {
    int x{0}, y{0};
    int width{0}, height{0};
};

struct ShaderStage {
    enum Type : uint8_t { Vertex, Fragment, Geometry } type{Vertex};
    std::string source{};        // GLSL/HLSL 源或文件路径，由后端解释
};

struct VertexElement {
    enum Format : uint8_t { Float2, Float3, Float4 } format{Float3};
    int semantic{0};             // 布局槽位（对应 location/binding）
    int offset{0};               // 相对顶点起始的字节偏移
    int stride{0};               // 顶点总字节步长
};

struct VertexLayout {
    std::vector<VertexElement> elements{};
};

struct DrawIndexedDesc {
    uint32_t indexCount{0};
    uint32_t indexOffset{0};
    uint32_t vertexOffset{0};
};

} // namespace rhi
