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
    enum Type : uint8_t { Vertex, Fragment, Geometry, Compute } type{Vertex};
    std::string source{};        // GLSL 源或文件路径（GL），SPIR-V 文件路径（Vulkan）
    std::string entry{"main"};   // 入口函数名
    bool sourceIsSPIRV{false};   // true=Vulkan 后端读 .spv
};

// ---- 顶点输入 ----
enum class VertexInputRate : uint8_t { PerVertex, PerInstance };

struct VertexElement {
    enum Format : uint8_t { Float2, Float3, Float4, Int4 } format{Float3};
    int semantic{0};             // 布局槽位（对应 location/binding）
    int binding{0};              // 顶点缓冲 binding 槽（GL 分离 VBO → Vulkan 多 binding）
    VertexInputRate inputRate{VertexInputRate::PerVertex};
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

// ---- 纹理 ----
enum class TextureFormat : uint8_t { RGB8, RGBA8, RGBA16F, RGB16F, RG16F, R32F, Depth32F, Depth24Stencil8 };
enum class TextureWrap : uint8_t { Repeat, ClampToEdge, ClampToBorder };
enum class TextureFilter : uint8_t { Linear, Nearest, LinearMipLinear };

struct TextureDesc {
    TextureFormat format{TextureFormat::RGBA8};
    TextureWrap wrapS{TextureWrap::Repeat};
    TextureWrap wrapT{TextureWrap::Repeat};
    TextureWrap wrapR{TextureWrap::Repeat};
    TextureFilter minFilter{TextureFilter::LinearMipLinear};
    TextureFilter magFilter{TextureFilter::Linear};
    bool generateMipmap{true};
    bool multisample{false};
    int samples{0};              // multisample 时 >1
};

// ---- 渲染目标 ----
enum class AttachmentType : uint8_t { Color, Depth, DepthStencil };

struct FramebufferAttachment {
    AttachmentType type{AttachmentType::Color};
    TextureFormat format{TextureFormat::RGBA8};
    bool external{false};        // true=由 App 提供纹理句柄，false=内部创建
    int samples{0};              // >0 时 MSAA
};

struct FramebufferDesc {
    int width{0};
    int height{0};
    int samples{0};              // FBO 级 MSAA 采样数（0=单采样）
    std::vector<FramebufferAttachment> attachments{};
};

// ---- 状态 ----
enum class CompareFunc : uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class StencilOp : uint8_t { Keep, Zero, Replace, Incr, Decr, IncrWrap, DecrWrap };
enum class BlendFactor : uint8_t { Zero, One, SrcAlpha, OneMinusSrcAlpha, SrcColor, OneMinusSrcColor };
enum class PolygonMode : uint8_t { Fill, Line, Point };
enum class CullFace : uint8_t { Back, Front, FrontAndBack };

struct StencilState {
    CompareFunc func{CompareFunc::Always};
    int reference{0};
    unsigned int mask{0xFF};
    StencilOp opFail{StencilOp::Keep};
    StencilOp opDepthFail{StencilOp::Keep};
    StencilOp opDepthPass{StencilOp::Keep};
};

struct BlendState {
    bool enable{false};
    BlendFactor src{BlendFactor::SrcAlpha};
    BlendFactor dst{BlendFactor::OneMinusSrcAlpha};
};

struct BackendCapabilities {
    int maxSamples{0};           // MSAA 最大采样数（0=不支持）
    size_t maxUniformBlockSize{0};
};

} // namespace rhi
