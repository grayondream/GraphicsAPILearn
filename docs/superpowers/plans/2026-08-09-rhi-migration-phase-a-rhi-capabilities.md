# 子项目 B 主体 — 计划A：RHI 能力补全 + 基类 RHI 化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 GL 后端当前缺失、会阻塞后续 46 App 迁移的 RHI 能力（图元类型、mat3 矩阵 uniform、RGBA32F、HDR 加载、Int4 整数指针），并把公共相机基类 `GLCameraBaseApp` RHI 化，作为后续所有 App 分批迁移的底座。

**Architecture:** 在现有 RHI 接口上**只新增方法/枚举值、不改既有签名**（GLPipeline/GLRenderer 是唯一实现，安全）。把 `PrimitiveType` 从"定义了但未接入"改为 GLRenderer 绘制时的拓扑状态；新增 `setUniformMatrix` 矩阵重载与 `RGBA32F` 格式；RhiImage 新增 HDR(RGB16F) 加载；修复顶点装配中 Int4 的整数指针语义；把 `GLCameraBaseApp` 改为继承 `App` 并清除其 native GL 依赖。

**Tech Stack:** C++17、OpenGL（glad）、stb_image（geometry::Image 已支持 `TextureOption{IsHdr}`）。

## Global Constraints

- **只新增不改既有接口签名**：`IRenderer::draw/drawIndexed/clearColor`、`IPipeline` 现有所有方法、`RhiImage::Load2D/LoadCube` 均保持可用（40 个未迁移 App 仍走旧路径）。
- 新增纯虚方法安全：GL 后端（`GLPipeline`/`GLRenderer`）是 `IPipeline`/`IRenderer` 唯一实现。
- 每任务末尾：`./scripts/build_run.sh build` 编译 + `./scripts/run.sh all -b gl -d 1` 全量 46/46 OK，再 `git commit`。
- 本计划**不重命名** `GLApp`/`GLCameraBaseApp` 类名（去前缀重命名留到全量迁移收尾计划统一做）；仅把 `GLCameraBaseApp` 的基类从 `GLApp` 改为 `App`（`src/app/App.hpp` 已是 `App : GLApp` 别名，无连锁影响）。
- 每次提交只含本任务改动，`git log` 保持线性，消息用 `feat(rhi): ...`。

---

### Task 1: 图元类型接入绘制路径（PrimitiveType + Points）

**Files:**
- Modify: `src/rhi/core/Common.hpp`
- Modify: `src/rhi/core/IPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.cpp`
- Modify: `src/rhi/gl/GLBackend.cpp`

**Interfaces:**
- Consumes: 现有 `PrimitiveType` 枚举、`IPipeline::use`、`GLRenderer::_pipeline`。
- Produces: `IPipeline::setPrimitiveType(PrimitiveType)`、`IPipeline::primitiveType() const`；`PrimitiveType` 增补 `Points`；`GLRenderer::draw*` 依据当前 pipeline 拓扑发出对应图元（`GL_TRIANGLES`/`GL_TRIANGLE_STRIP`/`GL_LINES`/`GL_POINTS`）。

- [ ] **Step 1: Common.hpp 增补 Points 枚举值**

在 `src/rhi/core/Common.hpp` 第 9 行：

```cpp
enum class PrimitiveType : uint8_t { TriangleList, TriangleStrip, Lines, Points };
```

- [ ] **Step 2: IPipeline 新增拓扑状态方法**

在 `src/rhi/core/IPipeline.hpp` 的状态命令区（`setMultisample` 后、顶点装配前）追加：

```cpp
    // 图元拓扑：决定 draw* 发出的 GL 图元
    virtual void setPrimitiveType(PrimitiveType type) = 0;
    virtual PrimitiveType primitiveType() const = 0;
```

- [ ] **Step 3: GLPipeline 实现拓扑状态**

`src/rhi/gl/GLPipeline.hpp` 私有成员区新增 `PrimitiveType _primitive{PrimitiveType::TriangleList};`，并声明两个 override。

`src/rhi/gl/GLPipeline.cpp`：

```cpp
void GLPipeline::setPrimitiveType(PrimitiveType type) { _primitive = type; }
PrimitiveType GLPipeline::primitiveType() const { return _primitive; }
```

- [ ] **Step 4: GLRenderer 绘制按拓扑发出图元**

在 `src/rhi/gl/GLBackend.cpp` 的 `GLRenderer` 内新增静态映射，并把 4 个 draw 方法改为使用拓扑（未 setPipeline 时默认 TriangleList）：

```cpp
static GLenum ToGLPrimitive(PrimitiveType t) {
    switch (t) {
        case PrimitiveType::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveType::Lines:         return GL_LINES;
        case PrimitiveType::Points:        return GL_POINTS;
        default:                           return GL_TRIANGLES;
    }
}
```

替换 `draw`/`drawIndexed`/`drawIndexedInstanced`/`drawInstanced` 的 `GL_TRIANGLES`：

```cpp
void draw(uint32_t vertexCount, uint32_t firstVertex) override {
    const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
    glDrawArrays(mode, firstVertex, vertexCount);
}
void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
    (void)vertexOffset;
    const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
    glDrawElements(mode, indexCount, GL_UNSIGNED_INT,
                   reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
}
void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
                          uint32_t indexOffset, uint32_t vertexOffset) override {
    (void)vertexOffset;
    const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
    glDrawElementsInstanced(mode, indexCount, GL_UNSIGNED_INT,
                            reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)), instanceCount);
}
void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) override {
    const GLenum mode = _pipeline ? ToGLPrimitive(_pipeline->primitiveType()) : GL_TRIANGLES;
    glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
}
```

- [ ] **Step 5: 编译 + 回归**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；46/46 OK（未迁移 App 默认 TriangleList 不变；新增纯虚方法由 GLPipeline 唯一实现）。

- [ ] **Step 6: Commit**

```bash
git add src/rhi/core/Common.hpp src/rhi/core/IPipeline.hpp src/rhi/gl/GLPipeline.hpp src/rhi/gl/GLPipeline.cpp src/rhi/gl/GLBackend.cpp
git commit -m "feat(rhi): wire PrimitiveType topology into GL draw path (add Points)"
```

---

### Task 2: mat3 矩阵 uniform + RGBA32F 纹理格式

**Files:**
- Modify: `src/rhi/core/IPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.cpp`
- Modify: `src/rhi/core/Common.hpp`
- Modify: `src/rhi/gl/GLHeader.hpp`

**Interfaces:**
- Consumes: 现有 `setUniform(name, ptr, count, vecSize)`（向量数组）。
- Produces: `IPipeline::setUniformMatrix(name, ptr, count, matSize)`（mat2/mat3/mat4 矩阵数组）；`TextureFormat` 增补 `RGBA32F`（→ `GL_RGBA32F`）。

- [ ] **Step 1: IPipeline 新增矩阵 uniform 方法**

在 `src/rhi/core/IPipeline.hpp` 的 `setUniform` 重载区追加（不改既有 4 个重载，避免与向量数组歧义）：

```cpp
    // 矩阵数组 uniform（matSize=2/3/4 → glUniformMatrix2fv/3fv/4fv）
    virtual bool setUniformMatrix(const std::string& name, const float* value, int count, int matSize) = 0;
```

- [ ] **Step 2: GLPipeline 实现矩阵 uniform**

在 `src/rhi/gl/GLPipeline.cpp`（`setUniform` 四重载之后）：

```cpp
bool GLPipeline::setUniformMatrix(const std::string& name, const float* value, int count, int matSize) {
    if (matSize == 3) glUniformMatrix3fv(Locate(*_shader, name), count, GL_FALSE, value);
    else if (matSize == 2) glUniformMatrix2fv(Locate(*_shader, name), count, GL_FALSE, value);
    else glUniformMatrix4fv(Locate(*_shader, name), count, GL_FALSE, value);
    return true;
}
```

- [ ] **Step 3: Common.hpp 增补 RGBA32F**

`src/rhi/core/Common.hpp` 第 46 行：

```cpp
enum class TextureFormat : uint8_t { RGB8, RGBA8, RGBA16F, RGB16F, RG16F, R32F, RGBA32F, Depth32F, Depth24Stencil8 };
```

- [ ] **Step 4: GLHeader 映射 RGBA32F**

`src/rhi/gl/GLHeader.hpp` 的 `ToGLInternalFormat` 追加：

```cpp
        case TextureFormat::RGBA32F:       return GL_RGBA32F;
```

`IsFloatFormat` 增补 `RGBA32F`：

```cpp
inline bool IsFloatFormat(TextureFormat f) {
    return f == TextureFormat::RGBA16F || f == TextureFormat::RGB16F ||
           f == TextureFormat::RG16F || f == TextureFormat::R32F ||
           f == TextureFormat::RGBA32F;
}
```

- [ ] **Step 5: 编译 + 回归**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；46/46 OK。

- [ ] **Step 6: Commit**

```bash
git add src/rhi/core/IPipeline.hpp src/rhi/gl/GLPipeline.cpp src/rhi/core/Common.hpp src/rhi/gl/GLHeader.hpp
git commit -m "feat(rhi): add mat3/mat2 matrix uniform overload and RGBA32F texture format"
```

---

### Task 3: RhiImage HDR（RGB16F）加载

**Files:**
- Modify: `src/app/GL/RhiImage.hpp`
- Modify: `src/app/GL/RhiImage.cpp`

**Interfaces:**
- Consumes: `geometry::Image(file, TextureOption{IsHdr=true})`（内部 `stbi_loadf`，`data()` 返回 `float*`）、`rhi::IRenderer::createTexture2D`、`ITexture2D::init(desc, view)`。
- Produces: `RhiImage::Load2DHDR(renderer, file)` → `std::shared_ptr<rhi::ITexture2D>`（RGB16F/RGBA16F，依据 channels）。

- [ ] **Step 1: 头文件声明**

`src/app/GL/RhiImage.hpp`：

```cpp
// 用 Image(stbi_loadf) 解码 HDR(.hdr) 为 float 数据，上传为 RGB16F/RGBA16F 2D 纹理
std::shared_ptr<rhi::ITexture2D> Load2DHDR(rhi::IRenderer* renderer, const std::string& file);
```

- [ ] **Step 2: 实现 Load2DHDR**

`src/app/GL/RhiImage.cpp`（在 `Load2D` 后）：

```cpp
std::shared_ptr<rhi::ITexture2D> Load2DHDR(rhi::IRenderer* renderer, const std::string& file) {
    Image img(file, TextureOption{true});
    img.load();
    if (!img.data()) {
        return {};
    }

    const int ch = img.size().channel;
    rhi::TextureDesc desc;
    desc.format = (ch == 4) ? rhi::TextureFormat::RGBA16F : rhi::TextureFormat::RGB16F;
    desc.wrapS = rhi::TextureWrap::ClampToEdge;
    desc.wrapT = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::LinearMipLinear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;

    rhi::TextureDataView2D view{img.data(), img.size().width, img.size().height, ch};
    auto tex = renderer->createTexture2D();
    tex->init(desc, view);
    return tex;
}
```

（`Image` 与 `TextureOption` 已由现有 `#include "geometry/Image.hpp"` 提供。）

- [ ] **Step 3: 编译**

```bash
./scripts/build_run.sh build
```

Expected: build OK。本任务新增函数未被任何 App 引用，仅验证可编译。

- [ ] **Step 4: Commit**

```bash
git add src/app/GL/RhiImage.hpp src/app/GL/RhiImage.cpp
git commit -m "feat(app): add RhiImage::Load2DHDR for HDR equirectangular (RGB16F) loading"
```

---

### Task 4: Int4 顶点装配用整数指针（修复 I-1 技术债）

**Files:**
- Modify: `src/rhi/gl/GLPipeline.cpp`

**Interfaces:**
- Consumes: `VertexElement::Int4`、`VertexLayout`。
- Produces: `GLPipeline::setVertexBuffer` 对 `Int4` 用 `glVertexAttribIPointer`（整数指针语义），消除蒙皮 BoneIDs 因浮点指针导致的精度错误。

- [ ] **Step 1: setVertexBuffer 按格式分流**

`src/rhi/gl/GLPipeline.cpp` 的 `setVertexBuffer` 循环内，把现有 `glVertexAttribPointer` 改为对 `Int4` 走整数指针：

```cpp
        for (const auto& e : _layout) {
            if (e.binding != static_cast<int>(binding)) continue;
            if (e.format == VertexElement::Int4) {
                glVertexAttribIPointer(e.semantic, 4, GL_INT, e.stride,
                                       reinterpret_cast<void*>(e.offset));
            } else {
                GLint size = 0;
                const GLenum type = ElementToGLFormat(e.format, &size);
                glVertexAttribPointer(e.semantic, size, type, GL_FALSE, e.stride,
                                      reinterpret_cast<void*>(e.offset));
            }
            glEnableVertexAttribArray(e.semantic);
            glVertexAttribDivisor(e.semantic,
                                  (e.inputRate == VertexInputRate::PerInstance) ? 1 : 0);
        }
```

- [ ] **Step 2: 编译 + 回归**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；46/46 OK（当前无 App 实际用 Int4 顶点，行为不变）。

- [ ] **Step 3: Commit**

```bash
git add src/rhi/gl/GLPipeline.cpp
git commit -m "fix(rhi): use glVertexAttribIPointer for Int4 vertex input (integer semantics)"
```

---

### Task 5: GLCameraBaseApp RHI 化（基类继承 App）

**Files:**
- Modify: `src/app/GL/Base/GLCameraBaseApp.hpp`
- Modify: `src/app/GL/Base/GLCameraBaseApp.cpp`

**Interfaces:**
- Consumes: `src/app/App.hpp`（`App : GLApp` 别名）、`geometry/Camera.hpp`。
- Produces: `GLCameraBaseApp` 不再 include `native/GL/GLProgram.hpp`/`glad`，基类从 `GLApp` 改为 `App`，可作为后续全部相机类 App 的 RHI 基类（类名不变，去前缀重命名留到收尾计划）。

- [ ] **Step 1: 头文件清理与改继承**

先读 `src/app/GL/Base/GLCameraBaseApp.cpp` 确认其未实际调用任何 GL API（相机/输入逻辑应为 API 无关）。`src/app/GL/Base/GLCameraBaseApp.hpp`：

```cpp
#pragma once
#include "app/App.hpp"
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include <memory>
#include <array>

class GLCameraBaseApp : public App {
```

（删除 `#include "native/GL/GLProgram.hpp"` 与 `class GLImageTexture2D;` 前置声明。）

- [ ] **Step 2: .cpp 清理**

`src/app/GL/Base/GLCameraBaseApp.cpp`：删除任何 `glad`/`GLProgram`/`GLImageTexture2D` include（若无实际 GL 调用则仅删 include）。保留相机/输入逻辑不变。

- [ ] **Step 3: 编译 + 回归**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；46/46 OK（GLCameraBaseApp 继承 App=GLApp 别名，其 30+ 派生类编译不受影响）。

- [ ] **Step 4: Commit**

```bash
git add src/app/GL/Base/GLCameraBaseApp.hpp src/app/GL/Base/GLCameraBaseApp.cpp
git commit -m "refactor(app): RHI-ize GLCameraBaseApp (drop native GL deps, inherit App)"
```

---

## Self-Review

**1. Spec coverage（对照 RHI 能力/缺口调研清单）**
- 图元类型未接入 draw（TRIANGLE_STRIP quad / POINTS 受阻）：Task 1 ✅
- mat3 矩阵 uniform 缺失（PBR normalMatrix）：Task 2 ✅
- RGBA32F 缺失（SSAO noise）：Task 2 ✅
- HDR 加载缺失（IBL 三个 App）：Task 3 ✅
- Int4 整数指针技术债 I-1（骨骼模型前置）：Task 4 ✅
- GLCameraBaseApp 公共基类 native GL 依赖：Task 5 ✅
- 明确**推迟到具体 App 批次**再补的能力（避免计划A过载、与消费方一同验证）：深度立方体贴图渲染目标（GLPointLightShadowApp）、`attachCubeFace` mip level（GLIBLSpecular prefilter）、仅清深度 clear（阴影 App）——将在 Light/PBR 批次计划内随 App 迁移引入。

**2. Placeholder scan**
- 无 TBD/实现略过。Task 1-5 均给出可直接落地的代码块或精确到行的修改点。Task 5 的 .cpp 以"删除 include、保留逻辑"指引，因需确认其无实际 GL 调用（执行时读文件核对）。
- 未引入未定义类型：`PrimitiveType`/`TextureFormat`/`TextureOption`/`glVertexAttribIPointer` 均为既有或本计划定义。

**3. Type consistency**
- Task 1 定义 `IPipeline::primitiveType() const` 返回 `PrimitiveType`，GLRenderer 用其驱动 `ToGLPrimitive`；`Points` 在 Common.hpp 增补后与 `ToGLPrimitive` 分支一致。
- Task 2 `setUniformMatrix(name, ptr, count, matSize)` 在 IPipeline 声明、GLPipeline 实现；`RGBA32F` 在 Common.hpp 增补、GLHeader 三处（ToGLInternalFormat/IsFloatFormat）映射一致。
- Task 3 `Load2DHDR` 返回 `shared_ptr<ITexture2D>`，`TextureDataView2D{data,w,h,ch}` 字段顺序与既有 Load2D 一致。
- Task 5 `GLCameraBaseApp : App`，与 `App.hpp` 定义（`App : GLApp`）一致。

**4. 收敛判断**
- 每任务只改各自文件，提交线性；新增纯虚方法均由 GL 后端唯一实现；回归红线 46/46 全程保持。
