# RHI 接口层扩展实施计划（子项目 A）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展 `src/rhi/core/` 接口层与 `src/rhi/gl/` GL 后端，使 46 个 App 的全部 GL 特性（geometry shader、instancing、多 MRT、深度纹理、cubemap attachment、MSAA resolve、显式 UBO、浮点纹理、运行时 stencil/blend/cull 状态）可被 RHI 表达，且现有 GL App 零改动回归通过。

**Architecture:** 一次设计完整接口集（依据 spec 的"46 App GL 特性全集"）。接口扩展分 6 个正交批次：①Common 基础类型 ②IBuffer+ITexture ③IPipeline 状态全集 ④IRenderTarget ⑤IRenderer ⑥GL 后端适配。每个批次新增接口后同步实现 GL 后端，保持编译通过 + `run.sh all -b gl` 全量回归。GL 后端是接口正确性的验证锚点。

**Tech Stack:** C++20, CMake, GLFW, glad (内置), glm, spdlog, vcpkg x64-linux。

## Global Constraints

- 所有 `gl*` / `GL_*` 调用**只能**出现在 `src/rhi/gl/` 下，App、base、geometry 一律不得直接依赖 GL。
- 接口层 `src/rhi/core/` 不得 `#include` 任何 GL 头。可依赖 glm 与基础头。
- 头文件 include 大小写与目录一致（`app/GL/GLApp.hpp`、`rhi/core/IRenderer.hpp` 等）。
- C++20 标准。`ErrorHandle::ExitIfFailed(ret, "msg")` 保留既有错误处理风格。
- 每个任务结束独立编译验证：`./scripts/build_run.sh build`；回归验证：`./scripts/run.sh all -b gl -d 1`。
- 设计文档：`docs/superpowers/specs/2026-08-09-vulkan-backend-design.md`。接口命名/职责以该文档为准。
- **回归红线**：本计划每步都不得破坏现有 GL App 的编译与渲染行为。现有 `GLApp::initGraphics()` 走 `_renderer->setViewport/setPipeline/clearColor/present`，`GLTriangleApp` 走 `setPipeline/setVertexBuffer/draw`——这些现有接口签名**不得改动**（只能新增），否则 46 个 App 全要跟着改，违反"接口新增不影响现有 App"。

---

## 文件结构总览

| 文件 | 职责 | 动作 |
|---|---|---|
| `src/rhi/core/Common.hpp` | 枚举（TextureFormat/AttachmentType/VertexInputRate/CompareFunc/StencilOp/BlendFactor/PolygonMode/TextureWrap/Filter）+ 结构体（FramebufferDesc/TextureDesc/StencilState/BlendState/ShaderStage 扩展） | Modify |
| `src/rhi/core/IBuffer.hpp` | `BufferType::Uniform`、`update()`、`bindRange()` | Modify |
| `src/rhi/core/ITexture2D.hpp` | `TextureDesc`、`init(desc,data)`、`createEmpty()` | Modify |
| `src/rhi/core/ITexture3D.hpp` | `initCube()`（6 面 cubemap）、`TextureDesc` | Modify |
| `src/rhi/core/IPipeline.hpp` | 状态命令全集 + `bindUniformBlock` + `setUniform` 扩展 | Modify |
| `src/rhi/core/IRenderTarget.hpp` | `FramebufferDesc` 版本 `create()`、`attachCubeFace()`、`colorTexture()/depthTexture()` 返回接口指针 | Modify |
| `src/rhi/core/IRenderer.hpp` | `createUniformBuffer`、`setRenderTarget`、`drawInstanced/drawIndexedInstanced`、`blitFramebuffer`、`bindTexture`(ITexture3D)、`backendCapabilities`、多 binding 的 setVertexBuffer 重载 | Modify |
| `src/rhi/core/ISwapchain.hpp` | 保持不变 | — |
| `src/rhi/core/ISurface.hpp` | 保持不变 | — |
| `src/rhi/core/IShader.hpp` | 保持不变（`ShaderStage` 已在 Common 扩展） | — |
| `src/rhi/gl/GLBuffer.cpp/.hpp` | Uniform 类型 + update + bindRange | Modify |
| `src/rhi/gl/GLTexture2D.cpp/.hpp` | TextureDesc 版本 init + createEmpty | Modify |
| `src/rhi/gl/GLTexture3D.cpp/.hpp` | initCube + TextureDesc | Modify |
| `src/rhi/gl/GLPipeline.cpp/.hpp` | 状态命令全集 + bindUniformBlock + setUniform 扩展 | Modify |
| `src/rhi/gl/GLRenderTarget.cpp/.hpp` | FramebufferDesc create + attachCubeFace + depthTexture | Modify |
| `src/rhi/gl/GLBackend.cpp/.hpp` | 新工厂/绘制/状态方法 | Modify |
| `src/rhi/gl/GLImageTexture2D.cpp` | 适配新 init 签名（可选，保留旧 init 则无需改） | Modify |
| `src/rhi/CMakeLists.txt` | 无需改（glob 自动收录） | — |

---

### Task 1: 扩展 Common.hpp 基础类型

**Files:**
- Modify: `src/rhi/core/Common.hpp`

**Interfaces:**
- Consumes: 现有 `PrimitiveType/Viewport/ShaderStage/VertexElement/VertexLayout/DrawIndexedDesc`（保持兼容）
- Produces: 下述全部新类型，供 Task 2-6 使用

- [ ] **Step 1: 重写 `src/rhi/core/Common.hpp`**

保留现有所有定义不动，追加以下内容（注意：`ShaderStage::Type` 的枚举值顺序不能变，新增 `Compute` 追加在末尾，避免影响 GL 后端 `ToGLType` 的 switch 与现有调用）：

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace rhi {

enum class PrimitiveType : uint8_t { TriangleList, TriangleStrip, Lines };  // 保留

struct Viewport {                                                            // 保留
    int x{0}, y{0};
    int width{0}, height{0};
};

struct ShaderStage {                                                         // 扩展
    enum Type : uint8_t { Vertex, Fragment, Geometry, Compute } type{Vertex};
    std::string source{};        // GLSL 源文件路径（GL）或 SPIR-V 文件路径（Vulkan）
    std::string entry{"main"};   // 入口函数名
    bool sourceIsSPIRV{false};   // true=Vulkan 后端读 .spv
};

// ---- 新增：顶点输入 ----
enum class VertexInputRate : uint8_t { PerVertex, PerInstance };

struct VertexElement {                                                       // 扩展
    enum Format : uint8_t { Float2, Float3, Float4, Int4 } format{Float3};
    int semantic{0};             // location
    int binding{0};              // 顶点缓冲 binding 槽（GL 分离 VBO → Vulkan 多 binding）
    VertexInputRate inputRate{VertexInputRate::PerVertex};
    int offset{0};
    int stride{0};
};

struct VertexLayout {                                                        // 保留
    std::vector<VertexElement> elements{};
};

// ---- 新增：纹理 ----
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

// ---- 新增：渲染目标 ----
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

// ---- 新增：状态 ----
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

struct DrawIndexedDesc {                                                     // 保留
    uint32_t indexCount{0};
    uint32_t indexOffset{0};
    uint32_t vertexOffset{0};
};

struct BackendCapabilities {
    int maxSamples{0};           // MSAA 最大采样数（0=不支持）
    size_t maxUniformBlockSize{0};
};

} // namespace rhi
```

- [ ] **Step 2: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 全量编译通过（现有 GL App 不受影响——本任务只追加类型，未改任何函数签名）。

- [ ] **Step 3: Commit**

```bash
git add src/rhi/core/Common.hpp
git commit -m "feat(rhi): extend Common with texture/framebuffer/state types"
```

---

### Task 2: 扩展 IBuffer + GLBuffer（Uniform / update / bindRange）

**Files:**
- Modify: `src/rhi/core/IBuffer.hpp`
- Modify: `src/rhi/gl/GLBuffer.hpp`
- Modify: `src/rhi/gl/GLBuffer.cpp`

**Interfaces:**
- Consumes: `BufferType`（现有）、Task 1 的 `rhi` 命名空间
- Produces: `IBuffer::update()`、`IBuffer::bindRange(binding, offset, size)`、`BufferType::Uniform`

- [ ] **Step 1: 扩展 `src/rhi/core/IBuffer.hpp`**

```cpp
#pragma once
#include "Common.hpp"
#include <cstdint>

namespace rhi {

enum class BufferType : uint8_t { Vertex, Index, Uniform };   // 追加 Uniform

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual bool init(const void* data, size_t size, BufferType type) = 0;
    virtual bool update(const void* data, size_t size, size_t offset = 0) = 0;  // 新增
    virtual bool bindRange(uint32_t binding, size_t offset, size_t size) = 0;    // 新增
    virtual bool bind() = 0;
    virtual void* handle() = 0;
};

} // namespace rhi
```

- [ ] **Step 2: 扩展 `src/rhi/gl/GLBuffer.hpp`**

在 `private:` 前加两行 override 声明；`private:` 内加 `GLenum targetFor() const;`：

```cpp
#pragma once
#include "rhi/core/IBuffer.hpp"
#include "GLHeader.hpp"
#include <cstdint>

namespace rhi {

class GLBuffer : public IBuffer {
public:
    ~GLBuffer();
    bool init(const void* data, size_t size, BufferType type) override;
    bool update(const void* data, size_t size, size_t offset) override;
    bool bindRange(uint32_t binding, size_t offset, size_t size) override;
    bool bind() override;
    void* handle() override;

private:
    GLenum targetFor() const;
    GLuint _id{0};
    BufferType _type{BufferType::Vertex};
    size_t _size{0};
};

} // namespace rhi
```

- [ ] **Step 3: 实现 `src/rhi/gl/GLBuffer.cpp`**

```cpp
#include "GLBuffer.hpp"

namespace rhi {

GLBuffer::~GLBuffer() {
    if (_id) glDeleteBuffers(1, &_id);
}

GLenum GLBuffer::targetFor() const {
    switch (_type) {
        case BufferType::Index:   return GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::Uniform: return GL_UNIFORM_BUFFER;
        case BufferType::Vertex:  break;
    }
    return GL_ARRAY_BUFFER;
}

bool GLBuffer::init(const void* data, size_t size, BufferType type) {
    _type = type;
    _size = size;
    if (!_id) glGenBuffers(1, &_id);
    glBindBuffer(targetFor(), _id);
    glBufferData(targetFor(), size, data, GL_STATIC_DRAW);
    return true;
}

bool GLBuffer::update(const void* data, size_t size, size_t offset) {
    if (!_id) return false;
    glBindBuffer(targetFor(), _id);
    glBufferSubData(targetFor(), static_cast<GLintptr>(offset), size, data);
    return true;
}

bool GLBuffer::bindRange(uint32_t binding, size_t offset, size_t size) {
    if (!_id) return false;
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, _id,
                      static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    return true;
}

bool GLBuffer::bind() {
    if (!_id) return false;
    glBindBuffer(targetFor(), _id);
    return true;
}

void* GLBuffer::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

} // namespace rhi
```

- [ ] **Step 4: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 全量编译通过（GLApp 只调 `init/bind`，现有接口签名未变，新增纯增量）。

- [ ] **Step 5: 回归验证 + Commit**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK（确认未破坏现有 App）。

```bash
git add src/rhi/core/IBuffer.hpp src/rhi/gl/GLBuffer.hpp src/rhi/gl/GLBuffer.cpp
git commit -m "feat(rhi): add uniform buffer support to IBuffer/GLBuffer (update/bindRange)"
```

---

### Task 3: 扩展 ITexture2D/ITexture3D + GL 实现（TextureDesc / createEmpty / cubemap）

**Files:**
- Modify: `src/rhi/core/ITexture2D.hpp`
- Modify: `src/rhi/core/ITexture3D.hpp`
- Modify: `src/rhi/gl/GLTexture2D.hpp` / `GLTexture2D.cpp`
- Modify: `src/rhi/gl/GLTexture3D.hpp` / `GLTexture3D.cpp`

**Interfaces:**
- Consumes: Task 1 的 `TextureDesc`/`TextureFormat`/`TextureWrap`/`TextureFilter`
- Produces: `ITexture2D::init(desc, data)`（新增重载，保留旧 `init(data)`）、`ITexture2D::createEmpty(desc)`、`ITexture3D::initCube(desc, faces[6])`

**设计要点**：**保留旧 `init(const TextureDataView2D&)` 重载不动**，新增带 desc 的重载——这样 `GLImageTexture2D::load()`（调用旧签名）和现有 App 零改动。cubemap 通过 `ITexture3D::initCube` 表达（Vulkan 中 cubemap 是 `VkImage` 维度=3，GL 中 `GL_TEXTURE_CUBE_MAP`）。

- [ ] **Step 1: 扩展 `src/rhi/core/ITexture2D.hpp`**

```cpp
#pragma once
#include <cstdint>
#include "Common.hpp"

namespace rhi {

struct TextureDataView2D {
    const void* data{nullptr};
    int width{0}, height{0};
    int channels{0};
};

class ITexture2D {
public:
    virtual ~ITexture2D() = default;
    virtual bool init(const TextureDataView2D& data) = 0;                    // 保留（旧签名）
    virtual bool init(const TextureDesc& desc, const TextureDataView2D& data) = 0;  // 新增
    virtual bool createEmpty(const TextureDesc& desc, int width, int height) = 0;   // 新增（渲染目标/深度纹理）
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

- [ ] **Step 2: 扩展 `src/rhi/core/ITexture3D.hpp`**

```cpp
#pragma once
#include <cstdint>
#include "Common.hpp"

namespace rhi {

struct TextureDataView3D {
    const void* data{nullptr};
    int width{0}, height{0}, depth{0};
    int channels{0};
};

class ITexture3D {
public:
    virtual ~ITexture3D() = default;
    virtual bool init(const TextureDataView3D& data) = 0;                    // 保留（旧签名）
    virtual bool initCube(const TextureDesc& desc,
                          const TextureDataView2D* faces) = 0;               // 新增：6 面 cubemap（faces[0..5]）
    virtual bool createEmpty(const TextureDesc& desc, int width, int height) = 0;   // 新增
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

- [ ] **Step 3: 实现 GLTexture2D 新方法（`src/rhi/gl/GLTexture2D.cpp`）**

在头文件 `GLTexture2D.hpp` 加两个 override 声明后，实现：

```cpp
// GLTexture2D.cpp 新增（保留旧 init 不动）
static GLenum ToGLInternalFormat(rhi::TextureFormat f) {
    using rhi::TextureFormat;
    switch (f) {
        case TextureFormat::RGB8:            return GL_RGB;
        case TextureFormat::RGBA8:           return GL_RGBA;
        case TextureFormat::RGBA16F:         return GL_RGBA16F;
        case TextureFormat::RGB16F:          return GL_RGB16F;
        case TextureFormat::RG16F:           return GL_RG16F;
        case TextureFormat::R32F:            return GL_R32F;
        case TextureFormat::Depth32F:        return GL_DEPTH_COMPONENT32F;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
    }
    return GL_RGBA;
}

static GLenum ToGLWrap(rhi::TextureWrap w) {
    switch (w) {
        case rhi::TextureWrap::Repeat:        return GL_REPEAT;
        case rhi::TextureWrap::ClampToEdge:   return GL_CLAMP_TO_EDGE;
        case rhi::TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
    }
    return GL_REPEAT;
}

static GLenum ToGLMinFilter(rhi::TextureFilter f) {
    switch (f) {
        case rhi::TextureFilter::Linear:        return GL_LINEAR;
        case rhi::TextureFilter::Nearest:       return GL_NEAREST;
        case rhi::TextureFilter::LinearMipLinear: return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

bool GLTexture2D::init(const TextureDesc& desc, const TextureDataView2D& data) {
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == rhi::TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    const auto internal = ToGLInternalFormat(desc.format);
    const bool isDepth = (desc.format == rhi::TextureFormat::Depth32F ||
                          desc.format == rhi::TextureFormat::Depth24Stencil8);
    const GLenum srcFmt = isDepth ? GL_DEPTH_COMPONENT : ((data.channels == 4) ? GL_RGBA : GL_RGB);
    const GLenum srcType = (desc.format == rhi::TextureFormat::RGBA16F ||
                            desc.format == rhi::TextureFormat::RGB16F ||
                            desc.format == rhi::TextureFormat::RG16F ||
                            desc.format == rhi::TextureFormat::R32F ||
                            desc.format == rhi::TextureFormat::Depth32F)
                           ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, data.width, data.height, 0, srcFmt, srcType, data.data);
    if (desc.generateMipmap) glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

bool GLTexture2D::createEmpty(const TextureDesc& desc, int width, int height) {
    _desc = desc;
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == rhi::TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    const auto internal = ToGLInternalFormat(desc.format);
    const bool isDepth = (desc.format == rhi::TextureFormat::Depth32F ||
                          desc.format == rhi::TextureFormat::Depth24Stencil8);
    const GLenum srcFmt = isDepth ? GL_DEPTH_COMPONENT : GL_RGBA;
    const GLenum srcType = (desc.format == rhi::TextureFormat::RGBA16F ||
                            desc.format == rhi::TextureFormat::RGB16F ||
                            desc.format == rhi::TextureFormat::RG16F ||
                            desc.format == rhi::TextureFormat::R32F ||
                            desc.format == rhi::TextureFormat::Depth32F)
                           ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, srcFmt, srcType, nullptr);
    return true;
}
```

注意：`TextureDesc` 不含宽高，所以 `createEmpty` 的宽高由调用方显式传入（`FramebufferDesc` 提供）。`GLTexture2D` 增加私有成员 `TextureDesc _desc{}` 记录最近一次描述，便于后续采样参数查询；宽高不持久化（接口调用即用）。

- [ ] **Step 4: 实现 GLTexture3D 的 initCube**

`GLTexture3D` 新增 `initCube`（绑定 `GL_TEXTURE_CUBE_MAP`，循环 6 面 `glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, ...)`）：

```cpp
bool GLTexture3D::initCube(const TextureDesc& desc, const TextureDataView2D* faces) {
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, _id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrapS));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrapT));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, ToGLWrap(desc.wrapR));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.minFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                    (desc.magFilter == rhi::TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR);
    for (int i = 0; i < 6; ++i) {
        const auto& f = faces[i];
        const auto internal = ToGLInternalFormat(desc.format);
        const bool isDepth = (desc.format == rhi::TextureFormat::Depth32F ||
                              desc.format == rhi::TextureFormat::Depth24Stencil8);
        const GLenum srcFmt = isDepth ? GL_DEPTH_COMPONENT : ((f.channels == 4) ? GL_RGBA : GL_RGB);
        const GLenum srcType = (desc.format == rhi::TextureFormat::Depth32F) ? GL_FLOAT : GL_UNSIGNED_BYTE;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal, f.width, f.height, 0, srcFmt, srcType, f.data);
    }
    return true;
}
```

`ToGLWrap`/`ToGLMinFilter`/`ToGLInternalFormat` 辅助函数放入 `GLTexture2D.cpp`（命名空间 rhi 内，static）。`GLTexture3D.cpp` 需要这些辅助——**为避免重复定义，把 3 个辅助函数移到 `GLHeader.hpp`**（`src/rhi/gl/GLHeader.hpp` 加 3 个 `inline` 函数），GLTexture2D/3D 共用。

- [ ] **Step 5: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 通过。若 `GLImageTexture2D` 等编译失败，检查是否误删旧签名（旧 `init(data)` 必须保留）。

- [ ] **Step 6: 回归验证 + Commit**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

```bash
git add src/rhi/core/ITexture2D.hpp src/rhi/core/ITexture3D.hpp \
        src/rhi/gl/GLHeader.hpp src/rhi/gl/GLTexture2D.* src/rhi/gl/GLTexture3D.*
git commit -m "feat(rhi): add texture desc/createEmpty/cubemap to ITexture2D/3D and GL backend"
```

---

### Task 4: 扩展 IPipeline 状态命令全集 + GL 实现

**Files:**
- Modify: `src/rhi/core/IPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.hpp` / `GLPipeline.cpp`
- Modify: `src/rhi/gl/GLShader.hpp` / `GLShader.cpp`（Step 3 加 uniform block 反射）

**Interfaces:**
- Consumes: Task 1 的 `CompareFunc/StencilOp/BlendFactor/PolygonMode/CullFace/StencilState/BlendState`
- Produces: `IPipeline` 全部状态命令 + `bindUniformBlock(binding)` + `setUniform` 的 vec2/vec3 数组重载

**关键**：现有 `setDepthTest/setCullMode/setBlend` 三个方法**保留签名**（`setCullMode(bool, int)`），因为 `GLApp` 或 App 若已调用会破坏。实际上探索显示 App 未调用（`GLApp` 只调 `_renderer->setPipeline`），但保持向后兼容零风险。新增方法与旧方法并存。

- [ ] **Step 1: 扩展 `src/rhi/core/IPipeline.hpp`**

```cpp
#pragma once
#include "Common.hpp"
#include <memory>

namespace rhi {

class IShader;

class IPipeline {
public:
    virtual ~IPipeline() = default;
    virtual void use() = 0;
    virtual void* handle() = 0;

    // ---- 便捷 uniform 层（GL 直连；Vulkan 用显式 UBO 为主，此层可映射或忽略）----
    virtual bool setUniform(const std::string& name, bool value) = 0;
    virtual bool setUniform(const std::string& name, int value) = 0;
    virtual bool setUniform(const std::string& name, float value) = 0;
    virtual bool setUniform(const std::string& name, const float* value, int count) = 0;   // mat4
    virtual bool setUniform(const std::string& name, const float* value, int count, int vecSize) = 0;  // vec2/3/4/1

    // ---- 显式 UBO ----
    virtual void bindUniformBlock(uint32_t binding) = 0;   // 声明本 pipeline 绑定到 UBO binding 槽

    // ---- 渲染状态（命令式全集）----
    // 以下旧三个方法保留，语义不变：
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCullMode(bool enable, int face) = 0;   // face: GL_FRONT/GL_BACK（旧）
    virtual void setBlend(bool enable) = 0;
    // 新增：
    virtual void setDepthFunc(CompareFunc func) = 0;
    virtual void setDepthMask(bool write) = 0;
    virtual void setStencilTest(bool enable) = 0;
    virtual void setStencilFunc(StencilFuncLike func, int ref, unsigned mask) = 0;  // 见下
    virtual void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) = 0;
    virtual void setStencilMask(unsigned mask) = 0;
    virtual void setBlendFunc(BlendFactor src, BlendFactor dst) = 0;
    virtual void setCullFaceEnable(bool enable) = 0;       // 新语义（与 setCullMode 并存）
    virtual void setCullFace(CullFace face) = 0;
    virtual void setFrontFace(bool ccw) = 0;               // true=CCW 正面，false=CW
    virtual void setPolygonMode(PolygonMode mode) = 0;
    virtual void setMultisample(bool enable) = 0;
};

} // namespace rhi
```

注意：`setStencilFunc(StencilFuncLike, ...)` 中的 `StencilFuncLike` 应为 `CompareFunc`（stencil func 与 depth func 同为 GL 比较函数）。**改用 `CompareFunc`**：

```cpp
virtual void setStencilFunc(CompareFunc func, int ref, unsigned mask) = 0;
```

- [ ] **Step 2: 实现 GLPipeline 新增方法（`src/rhi/gl/GLPipeline.cpp`）**

```cpp
#include "GLPipeline.hpp"
#include <glm/glm.hpp>

namespace rhi {

// 辅助映射
static GLenum ToGLCompare(rhi::CompareFunc f) {
    switch (f) {
        case rhi::CompareFunc::Never:        return GL_NEVER;
        case rhi::CompareFunc::Less:         return GL_LESS;
        case rhi::CompareFunc::Equal:        return GL_EQUAL;
        case rhi::CompareFunc::LessEqual:    return GL_LEQUAL;
        case rhi::CompareFunc::Greater:      return GL_GREATER;
        case rhi::CompareFunc::NotEqual:     return GL_NOTEQUAL;
        case rhi::CompareFunc::GreaterEqual: return GL_GEQUAL;
        case rhi::CompareFunc::Always:       return GL_ALWAYS;
    }
    return GL_ALWAYS;
}

static GLenum ToGLStencilOp(rhi::StencilOp op) {
    switch (op) {
        case rhi::StencilOp::Keep:      return GL_KEEP;
        case rhi::StencilOp::Zero:      return GL_ZERO;
        case rhi::StencilOp::Replace:   return GL_REPLACE;
        case rhi::StencilOp::Incr:      return GL_INCR;
        case rhi::StencilOp::Decr:      return GL_DECR;
        case rhi::StencilOp::IncrWrap:  return GL_INCR_WRAP;
        case rhi::StencilOp::DecrWrap:  return GL_DECR_WRAP;
    }
    return GL_KEEP;
}

static GLenum ToGLBlendFactor(rhi::BlendFactor f) {
    switch (f) {
        case rhi::BlendFactor::Zero:              return GL_ZERO;
        case rhi::BlendFactor::One:               return GL_ONE;
        case rhi::BlendFactor::SrcAlpha:          return GL_SRC_ALPHA;
        case rhi::BlendFactor::OneMinusSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
        case rhi::BlendFactor::SrcColor:          return GL_SRC_COLOR;
        case rhi::BlendFactor::OneMinusSrcColor:  return GL_ONE_MINUS_SRC_COLOR;
    }
    return GL_SRC_ALPHA;
}

static GLenum ToGLPolygonMode(rhi::PolygonMode m) {
    switch (m) {
        case rhi::PolygonMode::Fill:  return GL_FILL;
        case rhi::PolygonMode::Line:  return GL_LINE;
        case rhi::PolygonMode::Point: return GL_POINT;
    }
    return GL_FILL;
}

static GLenum ToGLFace(rhi::CullFace f) {
    switch (f) {
        case rhi::CullFace::Back:         return GL_BACK;
        case rhi::CullFace::Front:        return GL_FRONT;
        case rhi::CullFace::FrontAndBack: return GL_FRONT_AND_BACK;
    }
    return GL_BACK;
}

// 保留旧方法实现不动。新增：
void GLPipeline::setDepthFunc(rhi::CompareFunc func) { glDepthFunc(ToGLCompare(func)); }
void GLPipeline::setDepthMask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }
void GLPipeline::setStencilTest(bool enable) {
    if (enable) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}
void GLPipeline::setStencilFunc(rhi::CompareFunc func, int ref, unsigned mask) {
    glStencilFunc(ToGLCompare(func), ref, mask);
}
void GLPipeline::setStencilOp(rhi::StencilOp sfail, rhi::StencilOp dpfail, rhi::StencilOp dppass) {
    glStencilOp(ToGLStencilOp(sfail), ToGLStencilOp(dpfail), ToGLStencilOp(dppass));
}
void GLPipeline::setStencilMask(unsigned mask) { glStencilMask(mask); }
void GLPipeline::setBlendFunc(rhi::BlendFactor src, rhi::BlendFactor dst) {
    glBlendFunc(ToGLBlendFactor(src), ToGLBlendFactor(dst));
}
void GLPipeline::setCullFaceEnable(bool enable) {
    if (enable) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}
void GLPipeline::setCullFace(rhi::CullFace face) { glCullFace(ToGLFace(face)); }
void GLPipeline::setFrontFace(bool ccw) { glFrontFace(ccw ? GL_CCW : GL_CW); }
void GLPipeline::setPolygonMode(rhi::PolygonMode mode) {
    glPolygonMode(GL_FRONT_AND_BACK, ToGLPolygonMode(mode));
}
void GLPipeline::setMultisample(bool enable) {
    if (enable) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
}
void GLPipeline::bindUniformBlock(uint32_t binding) {
    if (!_shader) return;
    // 按 uniform block name 绑定到 binding 槽；name 从 shader 反射
    // GL 下需 App 提供 block 名。为通用，GLLocateBlock 用 shader 的 block 名列表。
    // 简化：GL 后端在 compile 时记录 block 名与 index；此处遍历绑定。
    // 见 Step 3。
}

} // namespace rhi
```

- [ ] **Step 3: `bindUniformBlock` 的 GL 实现细节**

GL 的 uniform block 需要知道 block 名才能 `glGetUniformBlockIndex`。**方案**：GLShader 在 `compile` 时遍历 `glGetProgramiv(GL_ACTIVE_UNIFORM_BLOCKS)`，记录 `name→index` 映射。`GLPipeline::bindUniformBlock(binding)` 遍历映射，对每个 block 调 `glUniformBlockBinding(index, binding)`。这需要 `GLShader` 暴露 block 映射。

修改 `GLShader.hpp`：

```cpp
class GLShader : public IShader {
public:
    ...
    const std::unordered_map<std::string, GLuint>& uniformBlocks() const { return _blocks; }
    void collectUniformBlocks();   // compile 成功后调用
private:
    GLuint compileStage(const ShaderStage& stage);
    GLuint _program{0};
    std::string _log{};
    std::unordered_map<std::string, GLuint> _blocks{};
};
```

`collectUniformBlocks` 实现（`GLShader.cpp` 的 `compile` 末尾成功后调用）：

```cpp
void GLShader::collectUniformBlocks() {
    _blocks.clear();
    if (!_program) return;
    GLint count = 0;
    glGetProgramiv(_program, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    for (GLint i = 0; i < count; ++i) {
        GLsizei len = 0;
        glGetActiveUniformBlockiv(_program, i, GL_UNIFORM_BLOCK_NAME_LENGTH, &len);
        std::string name(len > 0 ? len : 1, '\0');
        glGetActiveUniformBlockName(_program, i, len, nullptr, &name[0]);
        if (!name.empty()) name.pop_back();  // 去掉末尾 '\0'
        _blocks[name] = static_cast<GLuint>(i);
    }
}
```

`GLPipeline::bindUniformBlock` 最终实现：

```cpp
void GLPipeline::bindUniformBlock(uint32_t binding) {
    if (!_shader) return;
    for (const auto& [name, index] : _shader->uniformBlocks()) {
        (void)name;
        glUniformBlockBinding(_shader->id(), index, binding);
    }
}
```

（若未来需要逐 block 不同 binding，可加 `bindUniformBlock(name, binding)` 重载；本计划先用单一 binding 槽覆盖 GLUniformBufferApp 的 `Matrices` block 需求。）

- [ ] **Step 4: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 通过。

- [ ] **Step 5: 回归验证 + Commit**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

```bash
git add src/rhi/core/IPipeline.hpp src/rhi/gl/GLPipeline.hpp src/rhi/gl/GLPipeline.cpp src/rhi/gl/GLShader.hpp src/rhi/gl/GLShader.cpp
git commit -m "feat(rhi): add full pipeline state command set and uniform block binding"
```

---

### Task 5: 扩展 IRenderTarget + GL 实现（FramebufferDesc / attachCubeFace / 深度纹理）

**Files:**
- Modify: `src/rhi/core/IRenderTarget.hpp`
- Modify: `src/rhi/gl/GLRenderTarget.hpp` / `GLRenderTarget.cpp`

**Interfaces:**
- Consumes: Task 1 的 `FramebufferDesc/FramebufferAttachment/AttachmentType/TextureFormat`
- Produces: `IRenderTarget::create(const FramebufferDesc&)`（新重载）、`attachCubeFace()`、`colorTexture()/depthTexture()` 返回接口指针

**关键**：现有 `create(int,int)` 保留（`GLApp` 未用，但 `GLMsaaApp` 等未来迁移会用到新版本）。现有 `colorTexture()` 返回 `void*`——**保留该旧签名**，新增 `colorTexture2D(int)` 返回 `ITexture2D*` 与 `depthTexture2D()` 返回 `ITexture2D*`（避免破坏旧调用）。Vulkan 需要接口指针来采样。

- [ ] **Step 1: 扩展 `src/rhi/core/IRenderTarget.hpp`**

```cpp
#pragma once
#include <cstdint>
#include "Common.hpp"

namespace rhi {

class ITexture2D;
class ITexture3D;

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual bool create(int width, int height) = 0;                          // 保留（旧，默认 RGBA8+Depth）
    virtual bool create(const FramebufferDesc& desc) = 0;                    // 新增：多 attachment + samples
    virtual bool attachCubeFace(ITexture3D* cube, int face) = 0;             // 新增：IBL 动态挂接
    virtual bool bind() = 0;
    virtual bool unbind() = 0;
    virtual void* colorTexture() = 0;                                        // 保留（旧，返回 GLuint/VkImage 句柄）
    virtual ITexture2D* colorTexture2D(int attachment = 0) = 0;              // 新增：可采样接口指针
    virtual ITexture2D* depthTexture2D() = 0;                                // 新增：深度纹理可采样
    virtual bool resolveTo(IRenderTarget& dst) = 0;                          // 新增：MSAA blit resolve
    virtual void* handle() = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

- [ ] **Step 2: 实现 GLRenderTarget 新方法**

`GLRenderTarget` 增加成员：`std::vector<GLuint> _colorTexs{}`、`GLuint _depthTex{0}`（深度纹理，非 RBO）、`ITexture2D*` 适配。实现要点：

- `create(const FramebufferDesc& desc)`：按 desc 创建 N 个 color texture（格式映射同 Task 3 的 `ToGLInternalFormat`），`glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, ...)`；深度：若 desc 有 Depth/DepthStencil attachment 且 `external==false`，创建**深度纹理** `_depthTex`（`glTexImage2D(GL_DEPTH_COMPONENT)` 或 `GL_DEPTH24_STENCIL8`）挂 `GL_DEPTH_ATTACHMENT`；若 samples>0 则 `glRenderbufferStorageMultisample`（MSAA 用 RBO，resolve 由 `resolveTo` 做）。`glDrawBuffers` 设置 N 个 attachment。
- `resolveTo(dst)`：`glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo)` + `glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst)` + `glBlitFramebuffer(0,0,w,h,0,0,w,h, GL_COLOR_BUFFER_BIT, GL_NEAREST)`，然后恢复。
- `colorTexture2D(i)`：创建/返回一个包装 `_colorTexs[i]` 的 `GLTexture2D` 轻量对象（持有 GLuint 不删除）。
- `depthTexture2D()`：返回包装 `_depthTex` 的 `GLTexture2D`。
- `attachCubeFace(ITexture3D* cube, int face)`：`glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubeHandle, 0)`。

**包装纹理实现**：`GLRenderTarget` 内维护 `std::unique_ptr<GLTexture2D> _colorView[i]`、`_depthView`，`handle()` 返回 reinterpret_cast 自 `_fbo`。`colorTexture2D` 返回裸指针（所有权在 RT）。

```cpp
// GLRenderTarget.hpp 增补
#include "GLTexture2D.hpp"
#include <vector>
#include <memory>

class GLRenderTarget : public IRenderTarget {
public:
    ...
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face) override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;
private:
    GLuint _fbo{0}, _rbo{0}, _depthTex{0};
    std::vector<GLuint> _colorTexs{};
    std::vector<std::unique_ptr<GLTexture2D>> _colorViews{};
    std::unique_ptr<GLTexture2D> _depthView{};
    int _width{0}, _height{0};
    bool _msaa{false};
    int _samples{0};
};
```

实现中 `create(FramebufferDesc)` 复用旧 `create(int,int)` 的默认路径：当 desc.attachments 为空时退回旧逻辑；否则走新逻辑。

- [ ] **Step 3: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 通过。

- [ ] **Step 4: 回归验证 + Commit**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

```bash
git add src/rhi/core/IRenderTarget.hpp src/rhi/gl/GLRenderTarget.hpp src/rhi/gl/GLRenderTarget.cpp
git commit -m "feat(rhi): extend IRenderTarget with framebuffer desc, cube face attach, depth texture, resolve"
```

---

### Task 6: 扩展 IRenderer + GL 后端实现（instancing / render target / UBO 工厂 / blit / capabilities）

**Files:**
- Modify: `src/rhi/core/IRenderer.hpp`
- Modify: `src/rhi/gl/GLBackend.hpp` / `GLBackend.cpp`

**Interfaces:**
- Consumes: Task 1-5 的全部类型；`IRenderTarget` 新方法
- Produces: `IRenderer` 全部新工厂/绘制/状态方法（GL 后端实现）

- [ ] **Step 1: 扩展 `src/rhi/core/IRenderer.hpp`**

保留现有方法签名不动，追加：

```cpp
#pragma once
#include "Common.hpp"
#include <memory>
#include <cstdint>

namespace rhi {

class IBuffer;
class IShader;
class IPipeline;
class ITexture2D;
class ITexture3D;
class IRenderTarget;
class ISurface;
class ISwapchain;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // 生命周期（保留）
    virtual bool init(const std::shared_ptr<ISurface>& surface) = 0;
    virtual void shutdown() = 0;

    // 资源创建工厂（保留 + 新增）
    virtual std::shared_ptr<IShader> createShader() = 0;
    virtual std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) = 0;
    virtual std::shared_ptr<IBuffer> createBuffer() = 0;
    virtual std::shared_ptr<IBuffer> createUniformBuffer() = 0;               // 新增
    virtual std::shared_ptr<ITexture2D> createTexture2D() = 0;
    virtual std::shared_ptr<ITexture3D> createTexture3D() = 0;
    virtual std::shared_ptr<IRenderTarget> createRenderTarget() = 0;
    virtual std::shared_ptr<ISwapchain> getSwapchain() = 0;

    // 帧控制（保留）
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual bool present() = 0;

    // 状态与绘制（保留 + 新增）
    virtual void clearColor(float r, float g, float b, float a) = 0;
    virtual void setViewport(const Viewport& vp) = 0;
    virtual void setPipeline(const std::shared_ptr<IPipeline>& pipeline) = 0;
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;                                  // 保留（binding 0）
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) = 0;                // 新增：多 binding
    virtual void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;
    virtual void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) = 0;  // 新增：null=默认 framebuffer
    virtual void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit = 0) = 0;
    virtual void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit = 0) = 0;          // 新增：cubemap
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t indexOffset = 0, uint32_t vertexOffset = 0) = 0;
    virtual void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                                      uint32_t indexOffset = 0, uint32_t vertexOffset = 0) = 0;              // 新增
    virtual void drawInstanced(uint32_t vertexCount, uint32_t instanceCount,
                               uint32_t firstVertex = 0) = 0;                                                // 新增
    virtual void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                                 const std::shared_ptr<IRenderTarget>& dst) = 0;                              // 新增：MSAA resolve / depth 拷贝
    virtual BackendCapabilities backendCapabilities() = 0;                                                    // 新增
};

} // namespace rhi
```

- [ ] **Step 2: 实现 GLBackend 新方法**

`GLBackend.cpp` 的 `GLRenderer` 追加：

```cpp
std::shared_ptr<IBuffer> createUniformBuffer() override {
    return std::make_shared<GLBuffer>();
}

void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
    (void)binding;   // GL 下顶点属性绑定由 VAO 布局决定，binding 仅索引；此处忽略
    if (buffer) buffer->bind();
}

void setRenderTarget(const std::shared_ptr<IRenderTarget>& target) override {
    if (target) target->bind();
    else glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void bindTexture(const std::shared_ptr<ITexture3D>& texture, unsigned int unit) override {
    if (texture) texture->bind(unit);
}

void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                          uint32_t indexOffset, uint32_t vertexOffset) override {
    (void)vertexOffset;
    glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT,
                            reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)), instanceCount);
}

void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
    glDrawArraysInstanced(GL_TRIANGLES, firstVertex, vertexCount, instanceCount);
}

void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                     const std::shared_ptr<IRenderTarget>& dst) override {
    if (!src) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(reinterpret_cast<uintptr_t>(src->handle())));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dst->handle())) : 0);
    glBlitFramebuffer(0, 0, _viewportW, _viewportH, 0, 0, _viewportW, _viewportH,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

BackendCapabilities backendCapabilities() override {
    BackendCapabilities caps;
    GLint msaa = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &msaa);
    caps.maxSamples = msaa;
    caps.maxUniformBlockSize = 16384;  // GL 最低保证 16KB
    return caps;
}
```

需要：GLRenderer 增加 `int _viewportW{0}, _viewportH{0};`，在 `setViewport` 里记录；`blitFramebuffer` 用记录的窗口宽高。

- [ ] **Step 3: 编译验证**

Run: `./scripts/build_run.sh build`
Expected: 通过。

- [ ] **Step 4: 回归验证 + Commit**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

```bash
git add src/rhi/core/IRenderer.hpp src/rhi/gl/GLBackend.cpp src/rhi/gl/GLBackend.hpp
git commit -m "feat(rhi): extend IRenderer with instancing, render target binding, UBO factory, blit, capabilities"
```

---

### Task 7: 验证 GLShader 四阶段（Compute 预留）+ 编译/回归全量

**Files:**
- Modify: `src/rhi/gl/GLShader.cpp`（`ToGLType` 加 Compute 分支）
- Modify: `src/rhi/core/IShader.hpp`（无需改，`ShaderStage` 已含 Compute）

**Interfaces:**
- Consumes: Task 1 的 `ShaderStage::Compute`
- Produces: GL 后端能接受四阶段 shader（Compute 未使用则跳过）

- [ ] **Step 1: `ToGLType` 加 Compute 分支**

```cpp
static GLenum ToGLType(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        case ShaderStage::Compute:  return GL_COMPUTE_SHADER;
    }
    return GL_VERTEX_SHADER;
}
```

（现有 `GLShader::compile` 的三段式 `vs/fs/gs` 变量逻辑不处理 Compute——Compute 本计划不实现，仅接口预留。若某 stage 为 Compute，`compile` 里 `if/else` 链会忽略它但 `compileStage` 已编译——为安全，在 `compile` 循环里对 Compute 直接跳过并返回 false 提示未支持。）

- [ ] **Step 2: 编译 + 回归**

Run: `./scripts/build_run.sh build && ./scripts/run.sh all -b gl -d 1`
Expected: 全量 OK。

- [ ] **Step 3: Commit**

```bash
git add src/rhi/gl/GLShader.cpp
git commit -m "feat(rhi): support compute stage declaration in GL shader backend"
```

---

## Self-Review

**Spec 覆盖核对**：
- 多 binding 顶点缓冲（分离 VBO）→ Task 6 `setVertexBuffer(buffer, binding)` + Task 1 `VertexElement::binding` ✓
- instancing（实例属性 + divisor）→ Task 6 `drawIndexedInstanced/drawInstanced` + Task 1 `VertexInputRate::PerInstance` ✓
- 多 MRT → Task 5 `create(FramebufferDesc)` + `glDrawBuffers` ✓
- 深度纹理可采样 → Task 5 `depthTexture2D()` + Task 3 `TextureFormat::Depth32F` ✓
- cubemap attachment + 动态挂接 → Task 5 `attachCubeFace` ✓
- MSAA + resolve → Task 5 `resolveTo` + Task 6 `blitFramebuffer` ✓
- 显式 UBO → Task 2 `createUniformBuffer` + Task 4 `bindUniformBlock` ✓
- 浮点纹理 → Task 3 `TextureFormat` 全集 ✓
- geometry shader → Task 7 Compute 分支 + 现有 Geometry 支持 ✓
- 运行时 stencil/blend/cull 状态 → Task 4 命令全集 ✓
- 全屏 quad（TriangleStrip）→ `PrimitiveType::TriangleStrip` 已存在（Task 1 保留）✓
- 旧接口签名全部保留 → 各任务均注明"保留旧签名" ✓

**占位符扫描**：无 TBD/TODO；`bindUniformBlock` 设计明确（GLShader 反射 block 名）；`blitFramebuffer` 明确。`createEmpty` 签名在 Task 3 Step 3 已修正为 `(desc, width, height)`。

**类型一致性**：
- `CompareFunc` 用于 `setDepthFunc` 与 `setStencilFunc`（Step 1 已修正）✓
- `StencilState/BlendState` 结构体在 Task 1 定义，Task 4 未强制使用（命令式方法逐字段），保留结构体供 Vulkan 后端组合使用 ✓
- `createEmpty(desc, w, h)` 三处一致（接口声明 + GL 实现）✓
- `colorTexture2D(int)/depthTexture2D()` 返回 `ITexture2D*`，Task 5 头文件声明一致 ✓
