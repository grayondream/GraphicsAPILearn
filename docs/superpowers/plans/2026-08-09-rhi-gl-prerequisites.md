# RHI GL 后端前置补全 Implementation Plan（子项目 B 前置）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 RHI GL 后端缺失的顶点输入装配能力，打通 geometry/image → RHI 资源桥接，并迁移 5 个基础 App（Triangle/Rect/SimpleTexture/Cube/Camera）作为全量迁移（子项目 B 主体）的验证模板，同时让 Model/Mesh 脱离 glad/旧 GLProgram 依赖。

**Architecture:** 在 `GLPipeline` 上实现顶点输入装配（`use()` 绑定 VAO，`setVertexBuffer(buffer,binding)` 按 `VertexLayout.elements` 配置 `glVertexAttribPointer`），`GLRenderer` 改为向当前 pipeline 委派。新增 app 级 RHI 资源辅助（`RhiGeometry`/`RhiImage`），把 `geometry/Shape` 与 `geometry/Image` 上传为 RHI 缓冲/纹理。以 5 个基础 App 迁移验证管线正确性。Model/Mesh 改为消费 RHI 接口。

**Tech Stack:** C++17、OpenGL（glad）、glm、glad/assimp（Model 迁移用）、rhi core/gl 现有接口。

## Global Constraints

- 只新增不改既有接口签名：`IRenderer::draw/drawIndexed/setVertexBuffer(buffer)`、`IPipeline` 旧三个状态方法等必须保持可用（否则 40 个未迁移 App 崩溃）。
- GL 后端实现必须继续通过 `run.sh all -b gl -d 1` 全量 46 OK 的回归红线。
- 每任务末尾：`./scripts/build_run.sh build` 编译 + 对应 App 运行验证，再 `git commit`。
- 迁移期**不重命名** `GLApp`/`GLCameraBaseApp`/`AppType` 枚举/AppType 值（重命名放到全量迁移计划最后统一做，避免 40 个未迁移 App 连锁改动）。本计划只新增 `App`/`CameraBaseApp` 两个空别名基类，供已迁移 App 改继承，未迁移 App 仍继承 `GLApp`。
- 顶点数据沿用 GL 坐标约定（`Shape::toGL()` 输出），RHI 层不引入新的坐标系抽象。
- 命名：新增文件用 `Rhi` 前缀；新辅助放 `src/app/GL/`（App 迁移共享）。
- 每次提交只含本任务改动，`git log` 保持线性，消息用 `feat(rhi): ...`。

---

### Task 1: GL 后端顶点输入装配（GLPipeline + GLRenderer）

**Files:**
- Modify: `src/rhi/core/IPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.hpp`
- Modify: `src/rhi/gl/GLPipeline.cpp`
- Modify: `src/rhi/gl/GLBackend.cpp`

**Interfaces:**
- Produces: `IPipeline::setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t binding)`、`IPipeline::setIndexBuffer(const std::shared_ptr<IBuffer>&)`。`GLPipeline::use()` 现在同时绑定 VAO。`GLRenderer` 持有 `_pipeline` 并向其委派顶点缓冲绑定。

- [ ] **Step 1: 在 IPipeline 接口新增两个纯虚方法**

在 `src/rhi/core/IPipeline.hpp` 的状态命令区末尾追加：

```cpp
    // 顶点输入装配：按当前 VertexLayout 把 buffer 挂到指定 binding
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) = 0;
    virtual void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;
```

（保留既有 `setVertexBuffer(buffer)` 不删——它是 IRenderer 的，IPipeline 本来没有同名方法，无冲突。）

- [ ] **Step 2: 声明并实现 GLPipeline 的顶点装配**

在 `src/rhi/gl/GLPipeline.hpp` 私有方法区（`bindUniformBlock` 声明后）追加：

```cpp
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override;
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override;
```

在 `src/rhi/gl/GLPipeline.cpp` 实现。先把 `use()` 改为绑定 VAO：

```cpp
void GLPipeline::use() {
    if (_shader) glUseProgram(_shader->id());
    if (_vao) glBindVertexArray(_vao);
}
```

`bindShader` 保持只建空 VAO + 记布局（不动）。新增：

```cpp
static GLenum ElementToGLFormat(VertexElement::Format f, GLint* sizeOut) {
    switch (f) {
        case VertexElement::Float2: *sizeOut = 2; return GL_FLOAT;
        case VertexElement::Float3: *sizeOut = 3; return GL_FLOAT;
        case VertexElement::Float4: *sizeOut = 4; return GL_FLOAT;
        case VertexElement::Int4:   *sizeOut = 4; return GL_INT;
    }
    *sizeOut = 3; return GL_FLOAT;
}

void GLPipeline::setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) {
    if (_vao == 0) glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    if (buffer) buffer->bind();   // Vertex 缓冲 → GL_ARRAY_BUFFER
    for (const auto& e : _layout) {
        if (e.binding != static_cast<int>(binding)) continue;
        GLint size = 0;
        const GLenum type = ElementToGLFormat(e.format, &size);
        glVertexAttribPointer(e.semantic, size, type, GL_FALSE, e.stride,
                              reinterpret_cast<void*>(e.offset));
        glEnableVertexAttribArray(e.semantic);
        glVertexAttribDivisor(e.semantic,
                              (e.inputRate == VertexInputRate::PerInstance) ? 1 : 0);
    }
}

void GLPipeline::setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) {
    if (_vao == 0) glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    if (buffer) buffer->bind();   // Index 缓冲 → GL_ELEMENT_ARRAY_BUFFER（随 VAO 保存）
}
```

（`buffer->bind()` 依据 `GLBuffer::_type` 经 `targetFor()` 绑定正确 target；索引缓冲绑定到当前 VAO。）

- [ ] **Step 3: GLRenderer 持有当前 pipeline 并向其委派**

在 `src/rhi/gl/GLBackend.cpp` 的 `GLRenderer`：

新增私有成员：

```cpp
    std::shared_ptr<IPipeline> _pipeline{};
```

`setPipeline` 记录并 use：

```cpp
    void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
        _pipeline = pipeline;
        if (pipeline) pipeline->use();
    }
```

替换现有三个顶点/索引缓冲方法为委派：

```cpp
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (_pipeline) _pipeline->setVertexBuffer(buffer, 0);
    }
    void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer, uint32_t binding) override {
        if (_pipeline) _pipeline->setVertexBuffer(buffer, binding);
    }
    void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
        if (_pipeline) _pipeline->setIndexBuffer(buffer);
    }
```

- [ ] **Step 4: 编译 + 回归**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；全量 46/46 OK（未迁移 App 走旧路径，不受影响；新增 IPipeline 纯虚方法由 GLPipeline 唯一实现，无其它实现类）。

- [ ] **Step 5: Commit**

```bash
git add src/rhi/core/IPipeline.hpp src/rhi/gl/GLPipeline.hpp src/rhi/gl/GLPipeline.cpp src/rhi/gl/GLBackend.cpp
git commit -m "feat(rhi): implement vertex input assembly in GLPipeline (setVertexBuffer/setIndexBuffer)"
```

---

### Task 2: RHI 几何/图片资源辅助（RhiGeometry + RhiImage）

**Files:**
- Create: `src/app/GL/RhiGeometry.hpp`
- Create: `src/app/GL/RhiGeometry.cpp`
- Create: `src/app/GL/RhiImage.hpp`
- Create: `src/app/GL/RhiImage.cpp`

**Interfaces:**
- Consumes: `rhi::IRenderer`（createBuffer/createTexture2D/createTexture3D）、`geometry::Shape`（data/uv/normal/idx/byteSize/uvSize/normalSize/idxByteSize/size/idxSize/toGL）、`geometry::Image`（load + 数据访问）、`rhi::TextureDesc/TextureDataView`。
- Produces: `RhiGeometry::Geometry`（3 个顶点缓冲 + 可选索引缓冲 + `VertexLayout` + 计数）、`RhiGeometry::Create(IRenderer*, const Shape&, bool useUv, bool useNormal, bool useIndex)`、`RhiImage::Load2D(IRenderer*, const std::string&)` → `shared_ptr<ITexture2D>`、`RhiImage::LoadCube(IRenderer*, const std::string& dir)` → `shared_ptr<ITexture3D>`。

- [ ] **Step 1: 定义 RhiGeometry 结构与实现**

`src/app/GL/RhiGeometry.hpp`：

```cpp
#pragma once
#include "rhi/core/IRenderer.hpp"
#include <memory>

namespace geometry { class Shape; }

namespace RhiGeometry {
// 上传一个 geometry::Shape 到 RHI 缓冲，并生成对应的 VertexLayout。
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

Geometry Create(rhi::IRenderer* renderer, const geometry::Shape& shape,
                bool useUv, bool useNormal, bool useIndex);
}
```

`src/app/GL/RhiGeometry.cpp`：

```cpp
#include "RhiGeometry.hpp"
#include "geometry/Shape.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/Common.hpp"

namespace RhiGeometry {

Geometry Create(rhi::IRenderer* renderer, const geometry::Shape& shape,
                bool useUv, bool useNormal, bool useIndex) {
    using namespace rhi;
    Geometry g;

    g.vertexCount = shape.size();
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
        g.layout.elements.push_back(VertexElement{VertexElement::Float2, 2, 1,
                                                  VertexInputRate::PerVertex, 0, 8});
    }
    if (useNormal && shape.normalSize() > 0) {
        auto nb = renderer->createBuffer();
        nb->init(shape.normal(), shape.normalSize(), BufferType::Vertex);
        g.normalBuffer = nb;
        g.layout.elements.push_back(VertexElement{VertexElement::Float4, 3, 2,
                                                  VertexInputRate::PerVertex, 0, 16});
    }
    if (useIndex && shape.idxSize() > 0) {
        auto ib = renderer->createBuffer();
        ib->init(shape.idx(), shape.idxByteSize(), BufferType::Index);
        g.indexBuffer = ib;
        g.indexCount = shape.idxSize();
    }
    return g;
}

} // namespace RhiGeometry
```

注意：`shape.uv()`/`shape.normal()`/`shape.idx()` 返回 `const float*`/`const unsigned int*`，尺寸参数用对应 `*Size()` 的**字节数**；索引用 `idxByteSize()`。

- [ ] **Step 2: 实现 RhiImage（图片/立方体贴图加载）**

`src/app/GL/RhiImage.hpp`：

```cpp
#pragma once
#include "rhi/core/IRenderer.hpp"
#include <memory>
#include <string>

namespace RhiImage {
// 用 geometry::Image（stb）解码，上传为 RHI 2D 纹理
std::shared_ptr<rhi::ITexture2D> Load2D(rhi::IRenderer* renderer, const std::string& file);
// 从目录加载 right/left/top/bottom/front/back.jpg 六面，上传为 RHI cubemap
std::shared_ptr<rhi::ITexture3D> LoadCube(rhi::IRenderer* renderer, const std::string& dir);
}
```

`src/app/GL/RhiImage.cpp`：先读 `src/geometry/Image.hpp` 与 `src/native/ImageTexture2D.cpp` 确认 `geometry::Image` 的加载/数据访问 API（`load()`、像素数据与 `PixelFormat` 访问器、`width/height/channel`）。`Load2D` 流程：`Image img; if(!img.load(file)) return {};` → `renderer->createTexture2D()` → 构造 `rhi::TextureDesc`（`RGBA8`，wrap=ClampToEdge 或按原逻辑 Repeat，`LinearMipLinear`，`generateMipmap=true`）→ 用 `img` 的像素数据 + 尺寸构造 `rhi::TextureDataView`（参考 `src/native/ImageTexture2D.cpp` 中 `Texture2DDataView{data, size, format, size}` 的构造方式）→ `tex->init(desc, dataView)`。

`LoadCube` 流程：构造 `std::array<std::string,6>` 依次为 `<dir>/right.jpg,<dir>/left.jpg,<dir>/top.jpg,<dir>/bottom.jpg,<dir>/front.jpg,<dir>/back.jpg`（与 `src/native/ImageTexture3D.cpp` 的目录约定一致）→ 每面用 `Image` 解码得到 `TextureDataView` → `renderer->createTexture3D()` → `tex->initCube(desc, faces)`。

- [ ] **Step 3: 编译（CMake glob 自动收录新文件）**

```bash
./scripts/build_run.sh build
```

Expected: build OK。本任务新增代码未被任何 App 引用，仅验证可编译。

- [ ] **Step 4: Commit**

```bash
git add src/app/GL/RhiGeometry.hpp src/app/GL/RhiGeometry.cpp src/app/GL/RhiImage.hpp src/app/GL/RhiImage.cpp
git commit -m "feat(app): add RhiGeometry/RhiImage helpers bridging geometry/image to RHI"
```

---

### Task 3: 迁移 5 个基础 App 为 RHI 渲染管线模板

**Files:**
- Modify: `src/app/GL/Base/GLTriangleApp.hpp` / `GLTriangleApp.cpp`
- Modify: `src/app/GL/Base/GLRectApp.hpp` / `GLRectApp.cpp`
- Modify: `src/app/GL/Base/GLSimpleTextureApp.hpp` / `GLSimpleTextureApp.cpp`
- Modify: `src/app/GL/Base/GLCubeApp.hpp` / `GLCubeApp.cpp`
- Modify: `src/app/GL/Base/GLCameraApp.hpp` / `GLCameraApp.cpp`
- Create: `src/app/App.hpp`（空别名基类，见 Step 1）

**Interfaces:**
- Consumes: `renderer()->createShader/createPipeline/createBuffer/createTexture2D`、`rhi::IShader::compile`、`rhi::IPipeline::setUniform`、`renderer()->setPipeline/setVertexBuffer(buffer)/setVertexBuffer(buffer,binding)/setIndexBuffer/bindTexture/draw/drawIndexed`、`RhiGeometry::Create`、`RhiImage::Load2D`。
- Produces: 一个"迁移模板"，被后续全部 App 迁移复用。这些 App 仍继承 `GLApp`（名称不变），仅内部渲染序列改走 RHI。

- [ ] **Step 1: 新增 App / CameraBaseApp 空别名基类**

`src/app/App.hpp`：

```cpp
#pragma once
#include "app/GL/GLApp.hpp"
// 已迁移 App 的未来基类别名（全量迁移末尾会把 GLApp 整体重命名为 App）。
// 当前阶段仅作占位，保证 include 链平滑。
class App : public GLApp {
public:
    using GLApp::GLApp;
};
```

- [ ] **Step 2: 迁移 GLTriangleApp（单缓冲模板，全量贴出）**

`src/app/GL/Base/GLTriangleApp.hpp`：将私有成员从裸 `_vao/_vbo` 改为 RHI 资源，并去掉 `glad` 依赖：

```cpp
#pragma once
#include "app/App.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class GLTriangleApp : public App {
public:
    virtual ~GLTriangleApp();
protected:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;
private:
    std::shared_ptr<rhi::IPipeline> _pipeline{};
    std::shared_ptr<rhi::IBuffer> _vb{};
    rhi::VertexLayout _layout{};
    uint32_t _vertexCount{0};
};
```

`src/app/GL/Base/GLTriangleApp.cpp`（去 `glad`/`GLProgram` include，改用 RHI）：

```cpp
#include "GLTriangleApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "geometry/Triangle.hpp"
#include "RhiGeometry.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLTriangleApp::~GLTriangleApp() {}

bool GLTriangleApp::initApp() {
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.frag");

    auto shader = renderer()->createShader();
    auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
                                {rhi::ShaderStage::Fragment, ffile, "main", false} });
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

    Triangle shape{};
    auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, false);
    _layout = geo.layout;
    _vb = geo.vertexBuffer;
    _vertexCount = geo.vertexCount;

    _pipeline = renderer()->createPipeline(_layout, shader);
    return true;
}

void GLTriangleApp::drawScene(const float dt) {
    renderer()->setPipeline(_pipeline);
    renderer()->setVertexBuffer(_vb);
    renderer()->draw(_vertexCount, 0);
    return GLApp::drawScene(dt);
}
```

验证：`./scripts/run.sh all -b gl -a Triangle -d 2` → 三角形正常显示、无 GL 报错。

- [ ] **Step 3: 迁移 GLRectApp（交错 pos+color + 索引）**

读 `src/app/GL/Base/GLRectApp.cpp` 现有逻辑（Rect 用交错 pos+color 单 VBO + EBO，`glDrawElements`）。迁移要点：`RhiGeometry::Create(renderer(), Rect{}, false, false, true)` → `_layout`/`_vb`/`_ib`/`_indexCount`；`drawScene`：`setPipeline → setVertexBuffer(_vb) → setIndexBuffer(_ib) → drawIndexed(_indexCount, 0)`。去掉 `glad`/`GLProgram` include，改用 RHI。Rect 的 `toGL()` 在 RhiGeometry 内已处理。运行验证：`all -b gl -a Rect -d 2`。

- [ ] **Step 4: 迁移 GLSimpleTextureApp（pos+color + uv + 索引 + 纹理）**

读 `src/app/GL/Base/GLSimpleTextureApp.cpp` 现有逻辑。要点：
- 几何：`RhiGeometry::Create(renderer(), Rect{}, /*useUv=*/true, false, /*useIndex=*/true)` → 得到 `_vb`(binding0)、`_uv`(binding1)、`_ib`、`_indexCount`、`_layout`。
- 纹理：用 `RhiImage::Load2D(renderer().get(), join(StaticCollector::getImagePath(), "dog.jpg"))` 替代 `GLImageTexture2D`，存 `std::shared_ptr<rhi::ITexture2D> _texture{}`。
- 移除 `glViewport` 手动调用（GLApp::initGraphics 已 setViewport）与 `glad`/`GLImageTexture2D` include。
- `drawScene`：

```cpp
    renderer()->bindTexture(_texture, 0);
    renderer()->setPipeline(_pipeline);
    renderer()->setVertexBuffer(_vb);
    renderer()->setVertexBuffer(_uv, 1);
    renderer()->setIndexBuffer(_ib);
    renderer()->drawIndexed(_indexCount, 0);
    return GLApp::drawScene(dt);
```

运行验证：`all -b gl -a SimpleTexture -d 2` → 贴图正确、无错误。

- [ ] **Step 5: 迁移 GLCubeApp（pos+color + uv + normal + drawArrays）**

读 `src/app/GL/Base/GLCubeApp.cpp`。要点：Cube 画 36 顶点用 `glDrawArrays`（EBO 恒等、不用）。`RhiGeometry::Create(renderer(), Cube{}, /*useUv=*/true, /*useNormal=*/true, false)` → `_vb`(binding0)/`_uv`(binding1)/`_normal`(binding2)/`_layout`/`_vertexCount=36`。纹理用 `RhiImage::Load2D(dog.jpg)`。`glEnable(GL_DEPTH_TEST)` 改用 `_pipeline->setDepthTest(true)`（在 initApp 建 pipeline 后调用一次）。uniform：`_pipeline->setUniform("model", glm::value_ptr(model))` 等，矩阵用 `setUniform(name, const float*)`（GL 后端把它当 mat4）。drawScene：bindTexture + setPipeline + setVertexBuffer×3 + draw(36,0)。运行验证：`all -b gl -a Cube -d 2`。

- [ ] **Step 6: 迁移 GLCameraApp（相机 + 多立方体，模板基准）**

读 `src/app/GL/Base/GLCameraApp.cpp`。要点：继承关系 `GLApp`→改为 `App`（头文件 `class GLCameraApp : public App`）；`_camera`、输入事件逻辑不变。顶点装配同 GLCubeApp（3 VBO + drawArrays）。uniform 用 `_pipeline->setUniform("projection"/"view"/"model", glm::value_ptr(...))`。运行验证：`all -b gl -a Camera -d 3`（相机可移动）。

- [ ] **Step 7: 全量回归 + Commit**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；**全量 46/46 OK**（已迁移 5 个 App 走 RHI 且渲染正确，其余 41 个未迁移 App 仍走旧路径不受影响）。

```bash
git add src/app/App.hpp src/app/GL/Base/GLTriangleApp.* src/app/GL/Base/GLRectApp.* \
        src/app/GL/Base/GLSimpleTextureApp.* src/app/GL/Base/GLCubeApp.* src/app/GL/Base/GLCameraApp.*
git commit -m "refactor(app): migrate Triangle/Rect/SimpleTexture/Cube/Camera apps to RHI rendering"
```

---

### Task 4: Model/Mesh 去 GL（GLProgram→IShader/IPipeline + RHI 纹理）

**Files:**
- Modify: `src/model/Mesh.hpp` / `Mesh.cpp`
- Modify: `src/model/Model.hpp` / `Model.cpp`
- Modify: `src/app/GL/Model/GLLoadModelApp.hpp` / `GLLoadModelApp.cpp`（消费新接口）

**Interfaces:**
- Consumes: `rhi::IRenderer*`（createBuffer/createPipeline/createTexture2D/bindTexture/drawIndexed）、`RhiGeometry` 布局思路（Mesh 是单交错 VBO + 多属性，见下）。
- Produces: `Mesh`/`Model` 不再 include `<glad/glad.h>` 与 `native/GL/GLProgram.hpp`；`Mesh` 改为持 `rhi` 缓冲/纹理资源，`Model` 改为接受 `rhi::IRenderer*` 与 `rhi::IPipeline*` 进行绘制。

- [ ] **Step 1: 重构 Mesh 持 RHI 资源**

读 `src/model/Mesh.hpp/.cpp` 现有实现（`MeshVertex` 结构、`_vao/_vbo/_ebo`、`setupMesh` 属性 0-6：Position3/Normal3/TexCoords2/Tangent3/Bitangent3/BoneIDs-int4/Weights4，单交错 VBO `sizeof(MeshVertex)`≈88B）。改造：

- 移除 `<glad/glad.h>`、`native/GL/GLProgram.hpp` include。
- `Mesh` 成员改为 `std::shared_ptr<rhi::IBuffer> _vb{}, _ib{};`、`rhi::VertexLayout _layout;`、材质纹理 `std::vector<rhi::ITexture2D*>`（或 `std::shared_ptr<rhi::ITexture2D>`）替代裸 `unsigned int id`。
- 提供 `Mesh(rhi::IRenderer* renderer, std::vector<MeshVertex> vertices, std::vector<unsigned> indices, std::vector<Texture> textures)`，构造内 `setupMesh`：用 `renderer->createBuffer()` 上传交错顶点（`BufferType::Vertex`）与索引（`BufferType::Index`）；按 MeshVertex 字段构造 `VertexLayout`（7 个 element，semantic 0-6，offset 用 `offsetof(MeshVertex,...)`，stride=`sizeof(MeshVertex)`；BoneIDs 用 `VertexElement::Int4`、`inputRate=PerVertex`）。
- `draw` 签名改为 `void draw(rhi::IRenderer* renderer, rhi::IPipeline* pipeline, int count)`：循环材质纹理 `renderer->bindTexture(tex, unit)` + `pipeline->setUniform(type+序号, (int)unit)`；`renderer->setVertexBuffer(_vb); renderer->setIndexBuffer(_ib);` 然后 `count>1 ? renderer->drawIndexedInstanced(...) : renderer->drawIndexed(...)`。

注意：instancing（Saturn）用 `GLSaturnApp` 自行装配实例属性（divisor），Mesh 仅负责自身顶点；若 Saturn 需要，`VertexLayout` 的实例属性在 App 侧构造并合并，本任务保持 Mesh 为 PerVertex。

- [ ] **Step 2: 重构 Model 消费 rhi**

读 `src/model/Model.hpp/.cpp`（assimp 加载 + `TextureFromFile` 用 `GLImageTexture2D`）。改造：
- `Model(rhi::IRenderer* renderer, const std::string& path)` 保存 `_renderer`。
- `loadMaterialTextures` 用 `RhiImage::Load2D(renderer, filename)` 生成 `rhi::ITexture2D`，`textures_loaded` 去重逻辑保留。
- `draw(rhi::IRenderer*, rhi::IPipeline*)` 转发给各 Mesh。

- [ ] **Step 3: 迁移 GLLoadModelApp 消费新 Model 接口**

读 `src/app/GL/Model/GLLoadModelApp.cpp`。要点：`_program`（GLProgram）改 `_shader`/`_pipeline`（RHI）；`Model(renderer().get(), path)`；`drawScene`：`renderer()->setPipeline(_pipeline)` 后 `_model.draw(renderer().get(), _pipeline.get())`。initProgram 动态拼 shader 路径逻辑保留，改用 `rhi::ShaderStage`。运行验证：`all -b gl -a LoadModel -d 3`。

- [ ] **Step 4: 全量回归 + Commit**

```bash
./scripts/build_run.sh build
./scripts/run.sh all -b gl -d 1
```

Expected: build OK；46/46 OK（LoadModel 走新 Model 接口；Saturn/SSAO 暂未迁移仍用旧 Model 路径——若编译期因 Model/Mesh 签名改动报错，需同步最小化适配 Saturn/SSAO 对 Model 的调用，见 Step 5）。

- [ ] **Step 5: 适配剩余 Model 使用者（GLSaturnApp/GLSSAOApp）**

若 Step 4 编译因 `GLSaturnApp.cpp`/`GLSSAOApp.cpp` 调用旧 `Model::draw(GLProgram&)` 失败：在其调用处改为先 `renderer()->setPipeline(_pipeline)` 再 `_model.draw(renderer().get(), _pipeline.get())`，并保留其自身 GL 顶点装配逻辑（这两个 App 全量迁移在后续计划做）。改完重跑 Step 4 回归。

- [ ] **Step 6: Commit**

```bash
git add src/model src/app/GL/Model/GLLoadModelApp.* \
        src/app/GL/Advanced/Instance/GLSaturnApp.* src/app/GL/Light/Advanced/GLSSAOApp.*
git commit -m "refactor(model): decouple Model/Mesh from glad and legacy GLProgram, use RHI"
```

---

## Self-Review

**1. Spec coverage（对照设计文档 §9 GL 后端适配要点 + §App 迁移模式）**
- GL 后端顶点装配：Task 1（新增 IPipeline 顶点方法 + GLPipeline 实现）✅
- geometry/image→RHI 桥接：Task 2（RhiGeometry/RhiImage）✅
- 基础 App 迁移模板验证：Task 3（5 个基础 App）✅
- Model/Mesh 去 GL：Task 4（Mesh/Model + LoadModel）✅
- 去 GL 前缀命名（GLApp→App 等）：**本计划不执行**，明确归到全量迁移计划末尾统一做（Global Constraints 已声明），避免连锁改动 ✅（范围明确）

**2. Placeholder scan**
- Task 2 RhiImage.cpp 的具体 `geometry::Image` API 调用以"读现有 Image.hpp/ImageTexture2D.cpp 确认"指引替代，因精确 API 需执行时读文件确认；核心数据流已写清。Task 3/4 对 GLRectApp/GLCubeApp 等给出明确迁移要点而非完整代码——因为它们复刻 Task 2/3 已贴出的同构模板，且执行 agent 持有现有源码。均非空占位。
- 未出现 TBD/实现略过类文字。

**3. Type consistency**
- 跨任务引用的类型签名一致：`RhiGeometry::Create` 返回 `Geometry` 含 `layout: rhi::VertexLayout`，Task 3 用 `_layout=geo.layout` 传给 `createPipeline(_layout, shader)`；`VertexElement{format, semantic, binding, inputRate, offset, stride}` 字段顺序与 Common.hpp 定义一致（确认过）。
- `IPipeline::setVertexBuffer(buffer, binding)` 在 Task 1 定义、Task 3 drawScene 以 `setVertexBuffer(_uv, 1)` 调用，签名一致。
- `RhiImage::Load2D` 返回 `shared_ptr<ITexture2D>`，Task 3 存为 `shared_ptr<rhi::ITexture2D>`，一致。
- Task 4 Mesh 的 `VertexLayout` 与 `RhiGeometry` 布局用同一 `VertexElement` 结构，一致。
