# RHI 架构重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将约 50 个 `GLXxxApp` 重构为基于统一 RHI 抽象（`src/rhi/core/` + `src/rhi/gl/`）的 API 无关 App，使 Linux + OpenGL 全部跑通。

**Architecture:** 建立纯接口层 `src/rhi/core/`（IRenderer/IBuffer/IShader/IPipeline/ITexture2D/3D/IRenderTarget/ISwapchain/ISurface），`src/rhi/gl/` 内实现 GL 后端（所有 gl* 调用限定于此），App 只持有 `std::shared_ptr<IRenderer>` 并通过接口调用。GLFW 继续负责窗口，`Application` 通过 `IRenderer` 驱动渲染循环。

**Tech Stack:** C++20, CMake (>=3.20), GLFW (vcpkg), glad (内置), glm (vcpkg), spdlog, imgui (内置), vcpkg triplet x64-linux。

## Global Constraints

- 所有 `gl*` / `GL_*` 调用**只能**出现在 `src/rhi/gl/` 下，App、base、geometry 一律不得直接依赖 GL。
- 接口层 `src/rhi/core/` 不得 `#include` 任何 GL 头（glad、glcorearb、GLFW 的 GL 部分）。可依赖 glm 与基础头。
- 头文件 include 大小写必须与目录一致：`App/GL/GLApp.hpp`、`Native/GL/GLProgram.hpp` 等（源码目录已小写化为 `src/app/...`，但路径片段按代码内实际字符串）。
- C++20 标准。`ErrorHandle::ExitIfFailed(ret, "msg")` 保留既有错误处理风格。
- 每个任务结束可独立编译验证（目标平台 Linux，依赖 vcpkg `x64-linux`）。
- 设计文档：`docs/superpowers/specs/2026-08-08-rhi-architecture-design.md`。接口命名/职责以该文档为准。
- 迁移后 App 命名去掉 `GL` 前缀（`GLTriangleApp` → `TriangleApp`）。

---

## 现状与关键文件（执行前必读）

- `src/app/GL/GLApp.cpp`：`initGraphics()` 里做 `initGLContext`、`glViewport`、`glEnable(GL_DEPTH_TEST)`；`clearColor`/`beginDrawScene`/`drawScene`/`endDrawScene` 钩子，`endDrawScene` 里 `m_window->swapBuffers()`。
- `src/app/Application.cpp`：`init()` 创建 `GLFWWindow`；`run()` 循环里 `m_window->beginFrame()`/`pollEvents()`/`render()`/`endFrame()`；`render()` 调 `beginDrawScene`→`drawScene`→`endDrawScene`，并渲染 ImGui。
- `src/native/GL/GLProgram.{hpp,cpp}`：`init(vert,frag,geom)` 编译+链接程序；`use()`；`update(name, bool/int/float/vec3/vec4/mat3/mat4/vec数组)` 设 uniform；`uniformBind(name,binding)`；`locate()`。
- `src/native/GL/GLTexture2D.{hpp,cpp}`、`GLImageTexture2D`、`GLTexture3D`、`GLImageTexture3D`：纹理实现，均继承 `ITexture2D/ITexture3D`（在 `src/native/ITexture2D.hpp`）。
- `src/native/ITexture2D.hpp`：接口 `init(data)`/`bind(unit)`/`size()`/`release()`/`handle()`/`valid()`。
- `src/native/TextureBase.hpp`：`Texture2DBase : ITexture2D`，持有 `_size`。
- `src/app/GL/GLAppFactory.cpp`：~50 行 switch，`AppType`→`make_shared<GLXxxApp>()`。
- `src/app/AppFactory.cpp`：按 `GraphicsType` 分发到 `GLAppFactory::create`/`DX11AppFactory::create`。
- `src/geometry/Shape.hpp`：`toGL()` 顶点转换；`data()/idx()/uv()/normal()/byteSize()/idxByteSize()/uvSize()/size()`。
- `src/base/StaticCollector.hpp`：`getGLShaderPath()`、`getImagePath()`、`getModelPath()`。
- `src/app/GLFWWindow.hpp`：`initGLContext()`、`swapBuffers()`、`beginFrame()`、`endFrame()`、`getNativeGLFWWindow()`、`getProperties()`。
- `src/app/AppType.hpp`：`GraphicsType`、`AppType` 枚举。

---

## 环境准备（Task 0）

依赖已就绪：vcpkg `x64-linux` 已装 `glm/glfw/assimp/spdlog`，系统 `/usr/include/GL`、`/usr/lib/x86_64-linux-gnu/libGL*` 存在。构建目录 `build/` 已配置过但可能含过期缓存。执行前先重配以确认 OpenGL 可找到。

### Task 0: 确认构建环境可配置、可编译基线

**Files:**
- Verify: `/home/ares/workspace/GraphicsAPILearn/build/CMakeCache.txt`

**Interfaces:**
- Consumes: 现有源码 + vcpkg 依赖
- Produces: 一个能成功 configure + build 的基线

- [ ] **Step 1: 用 CMake 预设/直接配置，确认 OpenGL 能找到**

```bash
cd /home/ares/workspace/GraphicsAPILearn
cmake -S . -B build -DENABLE_OPENGL=ON
```

预期：configure 通过（`find_package(OpenGL REQUIRED)` 不再报错）。若仍报 OpenGL 缺失，说明环境仍未就绪，暂停并反馈，勿继续。

- [ ] **Step 2: 构建基线，确认现有代码可编译**

```bash
cmake --build build -j
```

预期：编译成功生成 `renderLearn`。若因缓存/目录大小写失败，清理 `build/` 后重试：
```bash
rm -rf build && cmake -S . -B build -DENABLE_OPENGL=ON && cmake --build build -j
```
记录是否有任何告警/错误，若有报错先停下反馈。

- [ ] **Step 3: 运行基线可执行程序（确认图形环境可用）**

```bash
./build/renderLearn
```
预期：打开窗口并显示当前 `main.cpp` 选中的 App（当前为 `PBR_IBL_Specular`）。窗口能显示即环境 OK。Ctrl+Esc 关闭。若 headless 无显示环境，记录并跳过本步。

- [ ] **Step 4: 提交无代码变更的环境确认（无需 commit）**

环境准备仅验证，不产生代码改动，无需 commit。进入 Task 1。

---

## RHI 核心接口层（src/rhi/core/）

### Task 1: 建立 rhi/core 纯接口层

**Files:**
- Create: `src/rhi/core/ISurface.hpp`
- Create: `src/rhi/core/ISwapchain.hpp`
- Create: `src/rhi/core/IShader.hpp`
- Create: `src/rhi/core/IPipeline.hpp`
- Create: `src/rhi/core/IBuffer.hpp`
- Create: `src/rhi/core/ITexture2D.hpp`
- Create: `src/rhi/core/ITexture3D.hpp`
- Create: `src/rhi/core/IRenderTarget.hpp`
- Create: `src/rhi/core/IRenderer.hpp`
- Create: `src/rhi/core/VertexLayout.hpp`
- Create: `src/rhi/core/Common.hpp`

**Interfaces:**
- Consumes: 设计文档中的接口职责表
- Produces: 所有接口类型定义（后续 GL 后端、App 依赖）

- [ ] **Step 1: 创建 `Common.hpp`（共享基础类型）**

`src/rhi/core/Common.hpp`：
```cpp
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
```

- [ ] **Step 2: 创建 `ISurface.hpp`**

`src/rhi/core/ISurface.hpp`：
```cpp
#pragma once

namespace rhi {

// 抽象原生平台表面，由后端/窗口适配器提供。
struct ISurface {
    virtual ~ISurface() = default;
    virtual void* nativeHandle() = 0;  // GL 返回 GLFWwindow*；后续 DX11 返回 HWND
    virtual int width() const = 0;
    virtual int height() const = 0;
};

} // namespace rhi
```

- [ ] **Step 3: 创建 `ISwapchain.hpp`**

`src/rhi/core/ISwapchain.hpp`：
```cpp
#pragma once
#include "Common.hpp"

namespace rhi {

class ISwapchain {
public:
    virtual ~ISwapchain() = default;
    virtual bool present() = 0;
    virtual void resize(int width, int height) = 0;
    virtual void* handle() = 0;   // 表面/原生句柄
};

} // namespace rhi
```

- [ ] **Step 4: 创建 `IShader.hpp`（编译与 IPipeline 分开）**

`src/rhi/core/IShader.hpp`：
```cpp
#pragma once
#include <string>
#include <vector>
#include "Common.hpp"

namespace rhi {

class IShader {
public:
    virtual ~IShader() = default;
    virtual bool compile(const std::vector<ShaderStage>& stages) = 0;
    virtual std::string getLog() const = 0;  // 编译失败时的后端日志
    virtual bool valid() const = 0;
};

} // namespace rhi
```

- [ ] **Step 5: 创建 `IPipeline.hpp`**

`src/rhi/core/IPipeline.hpp`：
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

    virtual bool setUniform(const std::string& name, bool value) = 0;
    virtual bool setUniform(const std::string& name, int value) = 0;
    virtual bool setUniform(const std::string& name, float value) = 0;
    virtual bool setUniform(const std::string& name, const float* value, int count) = 0;   // 矩阵/数组
    virtual bool setUniform(const std::string& name, const float* value, int count, int vecSize) = 0;

    // 渲染状态（显式暴露以便学习对比各 API 差异）
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCullMode(bool enable, int face) = 0;
    virtual void setBlend(bool enable) = 0;
};

} // namespace rhi
```

- [ ] **Step 6: 创建 `IBuffer.hpp`（顶点/索引缓冲，封装 vao/vbo/ebo）**

`src/rhi/core/IBuffer.hpp`：
```cpp
#pragma once
#include "Common.hpp"
#include <cstdint>

namespace rhi {

enum class BufferType : uint8_t { Vertex, Index };

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual bool init(const void* data, size_t size, BufferType type) = 0;
    virtual bool bind() = 0;
    virtual void* handle() = 0;
};

} // namespace rhi
```

- [ ] **Step 7: 创建 `ITexture2D.hpp` / `ITexture3D.hpp`（自 native 迁移，接口保持）**

`src/rhi/core/ITexture2D.hpp`：
```cpp
#pragma once
#include <cstdint>

namespace rhi {

struct TextureDataView2D {
    const void* data{nullptr};
    int width{0}, height{0};
    int channels{0};
};

class ITexture2D {
public:
    virtual ~ITexture2D() = default;
    virtual bool init(const TextureDataView2D& data) = 0;
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

`src/rhi/core/ITexture3D.hpp`：
```cpp
#pragma once
#include <cstdint>

namespace rhi {

struct TextureDataView3D {
    const void* data{nullptr};
    int width{0}, height{0}, depth{0};
    int channels{0};
};

class ITexture3D {
public:
    virtual ~ITexture3D() = default;
    virtual bool init(const TextureDataView3D& data) = 0;
    virtual void bind(unsigned int unit = 0) = 0;
    virtual void* handle() = 0;
    virtual bool valid() const = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

- [ ] **Step 8: 创建 `IRenderTarget.hpp`**

`src/rhi/core/IRenderTarget.hpp`：
```cpp
#pragma once
#include <cstdint>

namespace rhi {

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual bool create(int width, int height) = 0;
    virtual bool bind() = 0;
    virtual bool unbind() = 0;
    virtual void* colorTexture() = 0;   // 供采样/后处理读取
    virtual void* handle() = 0;
    virtual void release() = 0;
};

} // namespace rhi
```

- [ ] **Step 9: 创建 `IRenderer.hpp`（命令入口）**

`src/rhi/core/IRenderer.hpp`：
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

    // 生命周期
    virtual bool init(const std::shared_ptr<ISurface>& surface) = 0;
    virtual void shutdown() = 0;

    // 资源创建工厂（App 通过它获取资源）
    virtual std::shared_ptr<IShader> createShader() = 0;
    virtual std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) = 0;
    virtual std::shared_ptr<IBuffer> createBuffer() = 0;
    virtual std::shared_ptr<ITexture2D> createTexture2D() = 0;
    virtual std::shared_ptr<ITexture3D> createTexture3D() = 0;
    virtual std::shared_ptr<IRenderTarget> createRenderTarget() = 0;
    virtual std::shared_ptr<ISwapchain> getSwapchain() = 0;

    // 帧控制
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual bool present() = 0;

    // 状态与绘制
    virtual void clearColor(float r, float g, float b, float a) = 0;
    virtual void setViewport(const Viewport& vp) = 0;
    virtual void setPipeline(const std::shared_ptr<IPipeline>& pipeline) = 0;
    virtual void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;
    virtual void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) = 0;
    virtual void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit = 0) = 0;
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t indexOffset = 0, uint32_t vertexOffset = 0) = 0;
};

} // namespace rhi
```

- [ ] **Step 10: 编译验证（仅头文件，无源码）**

接口层全是头文件，无 .cpp。做一次"空编译"检查语法：
```bash
echo '#include "rhi/core/IRenderer.hpp"' > /tmp/rhi_check.cpp
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only /tmp/rhi_check.cpp
```
预期：无输出（编译通过）。若 glm 头被间接需要报错，属正常，先记录，接口层本身语法须正确。

- [ ] **Step 11: Commit**

```bash
cd /home/ares/workspace/GraphicsAPILearn
git add src/rhi/core/
git commit -m "feat(rhi): add core interface layer (ISurface/IShader/IPipeline/IBuffer/ITexture/IRenderTarget/IRenderer)"
```

---

## GL 后端（src/rhi/gl/）

### Task 2: 建立 rhi/gl 后端框架与基础类型

**Files:**
- Create: `src/rhi/gl/GLBackend.hpp` / `GLBackend.cpp`
- Create: `src/rhi/gl/GLHeader.hpp`
- Create: `src/rhi/gl/GLSwapchain.hpp` / `GLSwapchain.cpp`

**Interfaces:**
- Consumes: Task 1 的 `rhi::ISurface`、`rhi::ISwapchain`、`rhi::IRenderer`
- Produces: `GLBackend::create()`（返回 `std::shared_ptr<rhi::IRenderer>`）、`GLSwapchain`

- [ ] **Step 1: 创建 `GLHeader.hpp`（GL 唯一 include 点）**

`src/rhi/gl/GLHeader.hpp`：
```cpp
#pragma once
#include "glad/glad.h"
#include <GLFW/glfw3.h>
```
此文件是本后端**唯一**允许引入 `glad/glad.h` 与 GLFW GL 头的地方。

- [ ] **Step 2: 创建 `GLSwapchain.hpp`**

`src/rhi/gl/GLSwapchain.hpp`：
```cpp
#pragma once
#include "rhi/core/ISwapchain.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLSwapchain : public ISwapchain {
public:
    explicit GLSwapchain(GLFWwindow* window);
    bool present() override;
    void resize(int width, int height) override;
    void* handle() override;

private:
    GLFWwindow* _window{nullptr};
};

} // namespace rhi
```

- [ ] **Step 3: 创建 `GLSwapchain.cpp`**

`src/rhi/gl/GLSwapchain.cpp`：
```cpp
#include "GLSwapchain.hpp"

namespace rhi {

GLSwapchain::GLSwapchain(GLFWwindow* window) : _window(window) {}

bool GLSwapchain::present() {
    if (!_window) return false;
    glfwSwapBuffers(_window);
    return true;
}

void GLSwapchain::resize(int width, int height) {
    glViewport(0, 0, width, height);
}

void* GLSwapchain::handle() {
    return _window;
}

} // namespace rhi
```

- [ ] **Step 4: 创建 `GLBackend.hpp`**

`src/rhi/gl/GLBackend.hpp`：
```cpp
#pragma once
#include "rhi/core/IRenderer.hpp"

namespace rhi {

// 顶层工厂：创建 GL 渲染器
std::shared_ptr<IRenderer> createGLRenderer();

} // namespace rhi
```

- [ ] **Step 5: 创建 `GLBackend.cpp`（占位骨架，Task 5 填充完整）**

`src/rhi/gl/GLBackend.cpp`：
```cpp
#include "GLBackend.hpp"
#include "GLSwapchain.hpp"
#include "rhi/core/ISurface.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLRenderer final : public IRenderer {
public:
    // 本任务仅实现 init/getSwapchain 骨架，其余在本任务用 null 占位，
    // Task 5 中全部实现。此文件会持续演进。
    bool init(const std::shared_ptr<ISurface>& surface) override;
    void shutdown() override {}
    // ... 其余方法暂以 return false/nullptr 占位，见 Step 6 说明
};

std::shared_ptr<IRenderer> createGLRenderer() {
    return std::make_shared<GLRenderer>();
}

} // namespace rhi
```

说明：`GLRenderer` 是一个"不断演进"的类——Task 2 建骨架（init + 占位），后续 Task 3/4/5 逐步在 `GLRenderer` 内加入 `createTexture2D`、`createPipeline` 等实现。每个 Task 结束时 `GLRenderer` 必须**编译通过**，未实现的方法返回安全默认值（false/nullptr），不允许留下未定义符号。

- [ ] **Step 6: 先让 GLRenderer 完整占位（确保编译）**

`GLBackend.cpp` 的完整占位版（init 实现，其余方法返回默认值）：
```cpp
#include "GLBackend.hpp"
#include "GLSwapchain.hpp"
#include "rhi/core/ISurface.hpp"
#include "GLHeader.hpp"
#include <glm/glm.hpp>

namespace rhi {

class GLRenderer final : public IRenderer {
public:
    bool init(const std::shared_ptr<ISurface>& surface) override {
        _surface = surface;
        _swapchain = std::make_shared<GLSwapchain>(
            static_cast<GLFWwindow*>(_surface->nativeHandle()));
        return _swapchain != nullptr;
    }

    void shutdown() override {}

    std::shared_ptr<IShader> createShader() override { return nullptr; }
    std::shared_ptr<IPipeline> createPipeline(const VertexLayout&, const std::shared_ptr<IShader>&) override { return nullptr; }
    std::shared_ptr<IBuffer> createBuffer() override { return nullptr; }
    std::shared_ptr<ITexture2D> createTexture2D() override { return nullptr; }
    std::shared_ptr<ITexture3D> createTexture3D() override { return nullptr; }
    std::shared_ptr<IRenderTarget> createRenderTarget() override { return nullptr; }
    std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

    void beginFrame() override {}
    void endFrame() override {}
    bool present() override { return _swapchain ? _swapchain->present() : false; }
    void clearColor(float, float, float, float) override {}
    void setViewport(const Viewport&) override {}
    void setPipeline(const std::shared_ptr<IPipeline>&) override {}
    void setVertexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}
    void bindTexture(const std::shared_ptr<ITexture2D>&, unsigned int) override {}
    void draw(uint32_t, uint32_t) override {}
    void drawIndexed(uint32_t, uint32_t, uint32_t) override {}

private:
    std::shared_ptr<ISurface> _surface{};
    std::shared_ptr<ISwapchain> _swapchain{};
};

std::shared_ptr<IRenderer> createGLRenderer() {
    return std::make_shared<GLRenderer>();
}

} // namespace rhi
```

- [ ] **Step 7: 语法检查编译**

```bash
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only src/rhi/gl/GLBackend.cpp src/rhi/gl/GLSwapchain.cpp
```
预期：无输出。若 `glfw3.h`/`glad.h` 缺 include 路径，追加 `-I3party/glad/include`（视 glad 实际布局）。

- [ ] **Step 8: Commit**

```bash
git add src/rhi/gl/
git commit -m "feat(rhi): add GL backend skeleton and swapchain"
```

---

### Task 3: 实现 GLSwapchain/GLShader/GLPipeline

**Files:**
- Create: `src/rhi/gl/GLShader.hpp` / `GLShader.cpp`
- Create: `src/rhi/gl/GLPipeline.hpp` / `GLPipeline.cpp`

**Interfaces:**
- Consumes: `rhi::IShader`、`rhi::IPipeline`、`rhi::VertexLayout`（Task 1）；`GLProgram.cpp` 的编译逻辑（迁移源）
- Produces: `GLShader`（编译+日志）、`GLPipeline`（program use + uniform + 状态）

- [ ] **Step 1: 创建 `GLShader.hpp`**

`src/rhi/gl/GLShader.hpp`：
```cpp
#pragma once
#include "rhi/core/IShader.hpp"
#include "GLHeader.hpp"
#include <string>

namespace rhi {

class GLShader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return _program != 0; }
    GLuint id() const { return _program; }

private:
    GLuint compileStage(const ShaderStage& stage);
    GLuint _program{0};
    std::string _log{};
};

} // namespace rhi
```

- [ ] **Step 2: 创建 `GLShader.cpp`（迁移自 GLProgram.cpp 的编译逻辑）**

`src/rhi/gl/GLShader.cpp`：
```cpp
#include "GLShader.hpp"
#include "Base/Log.hpp"
#include "Utils/FileUtils.hpp"

namespace rhi {

static GLenum ToGLType(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLuint GLShader::compileStage(const ShaderStage& stage) {
    const auto src = FileUtils::readFile2String(stage.source);
    GLuint s = glCreateShader(ToGLType(stage.type));
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    int ok{};
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]{};
        glGetShaderInfoLog(s, 512, nullptr, buf);
        _log += buf;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool GLShader::compile(const std::vector<ShaderStage>& stages) {
    _log.clear();
    GLuint vs = 0, fs = 0, gs = 0;
    for (const auto& st : stages) {
        auto id = compileStage(st);
        if (id == 0) return false;
        if (st.type == ShaderStage::Vertex) vs = id;
        else if (st.type == ShaderStage::Fragment) fs = id;
        else if (st.type == ShaderStage::Geometry) gs = id;
    }
    _program = glCreateProgram();
    if (vs) glAttachShader(_program, vs);
    if (fs) glAttachShader(_program, fs);
    if (gs) glAttachShader(_program, gs);
    glLinkProgram(_program);
    int ok{};
    glGetProgramiv(_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]{};
        glGetProgramInfoLog(_program, 512, nullptr, buf);
        _log += buf;
        return false;
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (gs) glDeleteShader(gs);
    return true;
}

} // namespace rhi
```

- [ ] **Step 3: 创建 `GLPipeline.hpp`**

`src/rhi/gl/GLPipeline.hpp`：
```cpp
#pragma once
#include "rhi/core/IPipeline.hpp"
#include "GLShader.hpp"
#include <string>

namespace rhi {

class GLPipeline : public IPipeline {
public:
    void use() override;
    void* handle() override;
    void setDepthTest(bool enable) override;
    void setCullMode(bool enable, int face) override;
    void setBlend(bool enable) override;
    bool setUniform(const std::string& name, bool value) override;
    bool setUniform(const std::string& name, int value) override;
    bool setUniform(const std::string& name, float value) override;
    bool setUniform(const std::string& name, const float* value, int count) override;
    bool setUniform(const std::string& name, const float* value, int count, int vecSize) override;

    bool bindShader(const std::shared_ptr<GLShader>& shader, const VertexLayout& layout);

private:
    std::shared_ptr<GLShader> _shader{};
    GLuint _vao{0};
    GLuint _vbo{0};
    std::vector<VertexElement> _layout{};
};

} // namespace rhi
```

- [ ] **Step 4: 创建 `GLPipeline.cpp`**

`src/rhi/gl/GLPipeline.cpp`：
```cpp
#include "GLPipeline.hpp"
#include <glm/glm.hpp>

namespace rhi {

void GLPipeline::use() {
    if (_shader) glUseProgram(_shader->id());
}

void* GLPipeline::handle() {
    return _shader ? reinterpret_cast<void*>(static_cast<uintptr_t>(_shader->id())) : nullptr;
}

bool GLPipeline::bindShader(const std::shared_ptr<GLShader>& shader, const VertexLayout& layout) {
    _shader = shader;
    _layout = layout.elements;
    if (_vao == 0) glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    // 顶点缓冲由 GLRenderer 统一创建；此处记录布局
    return true;
}

void GLPipeline::setDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

void GLPipeline::setCullMode(bool enable, int face) {
    if (enable) { glEnable(GL_CULL_FACE); glCullFace(face); }
    else glDisable(GL_CULL_FACE);
}

void GLPipeline::setBlend(bool enable) {
    if (enable) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}

GLint Locate(const GLShader& s, const std::string& name) {
    return glGetUniformLocation(s.id(), name.c_str());
}

bool GLPipeline::setUniform(const std::string& name, bool value) {
    glUniform1i(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, int value) {
    glUniform1i(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, float value) {
    glUniform1f(Locate(*_shader, name), value); return true;
}
bool GLPipeline::setUniform(const std::string& name, const float* value, int count) {
    glUniformMatrix4fv(Locate(*_shader, name), count, GL_FALSE, value); return true;
}
bool GLPipeline::setUniform(const std::string& name, const float* value, int count, int vecSize) {
    if (vecSize == 3) glUniform3fv(Locate(*_shader, name), count, value);
    else if (vecSize == 4) glUniform4fv(Locate(*_shader, name), count, value);
    else if (vecSize == 2) glUniform2fv(Locate(*_shader, name), count, value);
    else glUniform1fv(Locate(*_shader, name), count, value);
    return true;
}

} // namespace rhi
```

- [ ] **Step 5: 编译验证**

```bash
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only src/rhi/gl/GLShader.cpp src/rhi/gl/GLPipeline.cpp
```
预期：无输出。若 `FileUtils::readFile2String` 位置不对，查 `src/utils/FileUtils.hpp` 修正命名空间。

- [ ] **Step 6: Commit**

```bash
git add src/rhi/gl/GLShader.* src/rhi/gl/GLPipeline.*
git commit -m "feat(rhi): add GL shader and pipeline backend"
```

---

### Task 4: 实现 GLBuffer / GLTexture2D / GLTexture3D / GLRenderTarget

**Files:**
- Create: `src/rhi/gl/GLBuffer.hpp` / `GLBuffer.cpp`
- Create: `src/rhi/gl/GLTexture2D.hpp` / `GLTexture2D.cpp`
- Create: `src/rhi/gl/GLTexture3D.hpp` / `GLTexture3D.cpp`
- Create: `src/rhi/gl/GLImageTexture2D.hpp` / `GLImageTexture2D.cpp`
- Create: `src/rhi/gl/GLImageTexture3D.hpp` / `GLImageTexture3D.cpp`
- Create: `src/rhi/gl/GLRenderTarget.hpp` / `GLRenderTarget.cpp`

**Interfaces:**
- Consumes: `rhi::IBuffer`、`rhi::ITexture2D/3D`、`rhi::IRenderTarget`（Task 1）；迁移源 `src/native/GL/GLTexture2D.cpp` 等
- Produces: GL 资源实现类（供 Task 5 的 GLRenderer 工厂返回）

- [ ] **Step 1: 创建 `GLBuffer.hpp` / `GLBuffer.cpp`**

`src/rhi/gl/GLBuffer.hpp`：
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
    bool bind() override;
    void* handle() override;

private:
    GLuint _id{0};
    BufferType _type{BufferType::Vertex};
};

} // namespace rhi
```

`src/rhi/gl/GLBuffer.cpp`：
```cpp
#include "GLBuffer.hpp"

namespace rhi {

GLBuffer::~GLBuffer() {
    if (_id) glDeleteBuffers(1, &_id);
}

bool GLBuffer::init(const void* data, size_t size, BufferType type) {
    _type = type;
    if (!_id) glGenBuffers(1, &_id);
    GLenum target = (_type == BufferType::Index) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, _id);
    glBufferData(target, size, data, GL_STATIC_DRAW);
    return true;
}

bool GLBuffer::bind() {
    GLenum target = (_type == BufferType::Index) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    glBindBuffer(target, _id);
    return true;
}

void* GLBuffer::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

} // namespace rhi
```

- [ ] **Step 2: 创建 `GLTexture2D.hpp` / `GLTexture2D.cpp`**

`src/rhi/gl/GLTexture2D.hpp`：
```cpp
#pragma once
#include "rhi/core/ITexture2D.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLTexture2D : public ITexture2D {
public:
    ~GLTexture2D();
    bool init(const TextureDataView2D& data) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _id != 0; }
    void release() override;

private:
    GLuint _id{0};
};

} // namespace rhi
```

`src/rhi/gl/GLTexture2D.cpp`：
```cpp
#include "GLTexture2D.hpp"

namespace rhi {

GLTexture2D::~GLTexture2D() { release(); }

bool GLTexture2D::init(const TextureDataView2D& data) {
    if (!_id) glGenTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, _id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum fmt = (data.channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, data.width, data.height, 0, fmt, GL_UNSIGNED_BYTE, data.data);
    glGenerateMipmap(GL_TEXTURE_2D);
    return true;
}

void GLTexture2D::bind(unsigned int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, _id);
}

void* GLTexture2D::handle() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(_id));
}

void GLTexture2D::release() {
    if (_id) { glDeleteTextures(1, &_id); _id = 0; }
}

} // namespace rhi
```

- [ ] **Step 3: 创建 `GLTexture3D.hpp` / `GLTexture3D.cpp`**（立方体/3D 纹理，参照 GLTexture2D，target 用 `GL_TEXTURE_CUBE_MAP` 或 `GL_TEXTURE_3D`，按原 GLTexture3D 语义实现）

`src/rhi/gl/GLTexture3D.hpp`：
```cpp
#pragma once
#include "rhi/core/ITexture3D.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLTexture3D : public ITexture3D {
public:
    ~GLTexture3D();
    bool init(const TextureDataView3D& data) override;
    void bind(unsigned int unit = 0) override;
    void* handle() override;
    bool valid() const override { return _id != 0; }
    void release() override;

private:
    GLuint _id{0};
};

} // namespace rhi
```
`GLTexture3D.cpp` 与 GLTexture2D.cpp 同理，target 用 `GL_TEXTURE_3D`，`glTexImage3D`。请对照 `src/native/GL/GLTexture3D.cpp` 实现精确匹配。

- [ ] **Step 4: 创建 `GLImageTexture2D.hpp` / `GLImageTexture2D.cpp`**（封装 stb 图片加载 + GLTexture2D，参照 `src/native/GL/GLImageTexture2D.cpp` 与 `src/native/ImageTexture2D.hpp`）

`src/rhi/gl/GLImageTexture2D.hpp`：
```cpp
#pragma once
#include "GLTexture2D.hpp"
#include <string>
#include <memory>

namespace rhi {

// 从文件加载图片到 GLTexture2D
class GLImageTexture2D {
public:
    GLImageTexture2D(const std::string& file, int reqChannels = 0);
    bool load();
    std::shared_ptr<GLTexture2D> texture() const { return _texture; }
    bool valid() const { return _texture && _texture->valid(); }

private:
    std::string _file{};
    int _reqChannels{0};
    std::shared_ptr<GLTexture2D> _texture{};
};

} // namespace rhi
```
`GLImageTexture2D.cpp`：用 stb_image 读文件→`TextureDataView2D`→`_texture->init()`。请参照 `src/native/GL/GLImageTexture2D.cpp` 的具体加载写法（stb 头引入在 3party/stbimage）。

- [ ] **Step 5: 创建 `GLImageTexture3D`**（立方体纹理，加载 6 面；如现有 `GLImageTexture3D` 为 3D/立方体用途则照搬逻辑到 `rhi::` 命名空间）

- [ ] **Step 6: 创建 `GLRenderTarget.hpp` / `GLRenderTarget.cpp`**（FBO + 颜色纹理 + RBO 深度）

`src/rhi/gl/GLRenderTarget.hpp`：
```cpp
#pragma once
#include "rhi/core/IRenderTarget.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLRenderTarget : public IRenderTarget {
public:
    ~GLRenderTarget();
    bool create(int width, int height) override;
    bool bind() override;
    bool unbind() override;
    void* colorTexture() override;
    void* handle() override { return reinterpret_cast<void*>(static_cast<uintptr_t>(_fbo)); }
    void release() override;

private:
    GLuint _fbo{0}, _colorTex{0}, _rbo{0};
    int _width{0}, _height{0};
};

} // namespace rhi
```
`GLRenderTarget.cpp`：`glGenFramebuffers`+`glGenTextures`(颜色，`GL_RGB`/`GL_RGBA16F` 视用途)+`glRenderbufferStorage`(深度)+`glCheckFramebufferStatus`；`bind()` 调 `glBindFramebuffer(GL_FRAMEBUFFER, _fbo)`；`unbind()` 调 `glBindFramebuffer(GL_FRAMEBUFFER, 0)`。请对照 `src/app/GL/Advanced/GLFrameBufferApp.cpp::createFrameBuffer()` 的精确参数。

- [ ] **Step 7: 编译验证所有新文件**

```bash
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only \
  src/rhi/gl/GLBuffer.cpp src/rhi/gl/GLTexture2D.cpp src/rhi/gl/GLTexture3D.cpp \
  src/rhi/gl/GLImageTexture2D.cpp src/rhi/gl/GLImageTexture3D.cpp src/rhi/gl/GLRenderTarget.cpp
```
预期：无输出。stb_image 需要 `.c` 或单 TU 定义，如编译报 undefined `stbi_load`，参考 3party/stbimage 的用法（可能需 `#define STB_IMAGE_IMPLEMENTATION` 于某处）。

- [ ] **Step 8: Commit**

```bash
git add src/rhi/gl/GLBuffer.* src/rhi/gl/GLTexture2D.* src/rhi/gl/GLTexture3D.* src/rhi/gl/GLImageTexture2D.* src/rhi/gl/GLImageTexture3D.* src/rhi/gl/GLRenderTarget.*
git commit -m "feat(rhi): add GL buffer/texture/render-target backend"
```

---

### Task 5: 完成 GLRenderer 工厂并接线全部资源

**Files:**
- Modify: `src/rhi/gl/GLBackend.cpp`（把占位方法替换为真实工厂实现）
- Modify: `src/rhi/gl/GLBackend.hpp`（无需改，仍导出 `createGLRenderer`）

**Interfaces:**
- Consumes: Task 2/3/4 的全部 GL 类
- Produces: 完整 `IRenderer` 实现（App 可直接用）

- [ ] **Step 1: 实现 GLRenderer 全部工厂方法与绘制**

在 `GLBackend.cpp` 的 `GLRenderer` 中，把占位替换为：
```cpp
std::shared_ptr<IShader> createShader() override { return std::make_shared<GLShader>(); }
std::shared_ptr<IPipeline> createPipeline(const VertexLayout& layout, const std::shared_ptr<IShader>& shader) override {
    auto p = std::make_shared<GLPipeline>();
    auto gls = std::dynamic_pointer_cast<GLShader>(shader);
    p->bindShader(gls, layout);
    return p;
}
std::shared_ptr<IBuffer> createBuffer() override { return std::make_shared<GLBuffer>(); }
std::shared_ptr<ITexture2D> createTexture2D() override { return std::make_shared<GLTexture2D>(); }
std::shared_ptr<ITexture3D> createTexture3D() override { return std::make_shared<GLTexture3D>(); }
std::shared_ptr<IRenderTarget> createRenderTarget() override { return std::make_shared<GLRenderTarget>(); }
std::shared_ptr<ISwapchain> getSwapchain() override { return _swapchain; }

void clearColor(float r, float g, float b, float a) override {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}
void setViewport(const Viewport& vp) override { glViewport(vp.x, vp.y, vp.width, vp.height); }
void setPipeline(const std::shared_ptr<IPipeline>& pipeline) override {
    if (pipeline) pipeline->use();
}
void setVertexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
    if (buffer) buffer->bind();
}
void setIndexBuffer(const std::shared_ptr<IBuffer>& buffer) override {
    if (buffer) buffer->bind();
}
void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit) override {
    if (texture) texture->bind(unit);
}
void draw(uint32_t vertexCount, uint32_t firstVertex) override {
    glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
}
void drawIndexed(uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset) override {
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
}
```
`init` 已实现。补 `#include "GLShader.hpp"` `#include "GLPipeline.hpp"` `#include "GLBuffer.hpp"` `#include "GLTexture2D.hpp"` `#include "GLTexture3D.hpp"` `#include "GLRenderTarget.hpp"`。

- [ ] **Step 2: 编译验证**

```bash
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only src/rhi/gl/GLBackend.cpp
```
预期：无输出。

- [ ] **Step 3: Commit**

```bash
git add src/rhi/gl/GLBackend.cpp
git commit -m "feat(rhi): complete GL renderer factory wiring"
```

---

### Task 6: 接入 CMake，编译 rhi/gl 进 renderLearn

**Files:**
- Modify: `src/CMakeLists.txt`
- Create: `src/rhi/CMakeLists.txt`

**Interfaces:**
- Consumes: 现有 src 子目录 CMake 约定
- Produces: `rhi/core` + `rhi/gl` 被 `add_subdirectory` 纳入构建，`ALL_CPP_FILES` 收录其 .cpp

- [ ] **Step 1: 阅读现有 add_source_group 与子目录收集机制**

查看 `src/BuildPost.cmake` 与 `src/app/CMakeLists.txt`、`src/native/CMakeLists.txt` 如何把源文件收进 `ALL_CPP_FILES`。rhi 目录按同样模式接入。

- [ ] **Step 2: 创建 `src/rhi/CMakeLists.txt`**

`src/rhi/CMakeLists.txt`：
```cmake
set(RENDER_RHI_PATH "${RENDER_SOURCE_PATH}/rhi")
add_source_group(${RENDER_RHI_PATH} "")

if(ENABLE_OPENGL)
    add_source_group(${RENDER_RHI_PATH}/gl "")
endif()
```
（`add_source_group` 内部会把文件追加到 `ALL_CPP_FILES`/`ALL_HPP_FILES`，与现有 native 一致。）

- [ ] **Step 3: 在 `src/CMakeLists.txt` 增加 add_subdirectory**

在 `add_subdirectory(${RENDER_GEOMETRY_PATH})` 之后插入：
```cmake
add_subdirectory(${RENDER_SOURCE_PATH}/rhi)
```

- [ ] **Step 4: 全量构建验证**

```bash
cd /home/ares/workspace/GraphicsAPILearn
cmake -S . -B build -DENABLE_OPENGL=ON
cmake --build build -j
```
预期：编译通过，`rhi/gl` 源被编译进 `renderLearn`。

- [ ] **Step 5: Commit**

```bash
git add src/CMakeLists.txt src/rhi/CMakeLists.txt
git commit -m "build(rhi): integrate rhi/core and rhi/gl into build"
```

---

## 集成：GLApp/Application 改用 IRenderer

### Task 7: 引入 GLFWSurface 适配器 + IRenderer 创建入口

**Files:**
- Create: `src/rhi/gl/GLFWSurface.hpp` / `GLFWSurface.cpp`（或放 `src/app`，见下）
- Modify: `src/app/GLFWWindow.hpp` / `GLFWWindow.cpp`（可选，提供 surface）

**Interfaces:**
- Consumes: `rhi::ISurface`
- Produces: 一个把 `GLFWwindow*` 包装成 `rhi::ISurface` 的适配器

- [ ] **Step 1: 创建 `GLFWSurface`（实现 ISurface）**

`src/rhi/gl/GLFWSurface.hpp`：
```cpp
#pragma once
#include "rhi/core/ISurface.hpp"
#include "GLHeader.hpp"

namespace rhi {

class GLFWSurface : public ISurface {
public:
    explicit GLFWSurface(GLFWwindow* window, int w, int h) : _window(window), _w(w), _h(h) {}
    void* nativeHandle() override { return _window; }
    int width() const override { return _w; }
    int height() const override { return _h; }

private:
    GLFWwindow* _window{nullptr};
    int _w{0}, _h{0};
};

} // namespace rhi
```
`GLFWSurface.cpp` 可为空（全 inline），或省略 cpp。若需 cpp，只放 `#include "GLFWSurface.hpp"`。

- [ ] **Step 2: 编译验证**

```bash
g++ -std=c++20 -Isrc -I/home/ares/apps/vcpkg/installed/x64-linux/include -fsyntax-only src/rhi/gl/GLFWSurface.hpp
```
预期：无输出。

- [ ] **Step 3: Commit**

```bash
git add src/rhi/gl/GLFWSurface.hpp
git commit -m "feat(rhi): add GLFW surface adapter implementing ISurface"
```

---

### Task 8: GLApp 通过 IRenderer 初始化与绘制

**Files:**
- Modify: `src/app/GL/GLApp.hpp`
- Modify: `src/app/GL/GLApp.cpp`

**Interfaces:**
- Consumes: Task 7 的 `GLFWSurface`、Task 5 的 `createGLRenderer()`
- Produces: `GLApp` 持有 `std::shared_ptr<rhi::IRenderer> _renderer`，提供 `renderer()` 访问器；`initGraphics`/`clearColor`/`endDrawScene` 走 RHI

- [ ] **Step 1: 修改 `GLApp.hpp` 增加 renderer 成员与访问器**

`src/app/GL/GLApp.hpp`（在 `#include "App/Application.hpp"` 后加 `#include "rhi/core/IRenderer.hpp"`）：
```cpp
#pragma once
#include "App/Application.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class GLApp : public Application {
public:
    GLApp();
    ~GLApp();

    std::shared_ptr<rhi::IRenderer> renderer() const { return _renderer; }

protected:
    virtual bool initGraphics() override;
    virtual void clearColor() override;
    virtual void beginDrawScene() override;
    virtual void drawScene(const float dt) override;
    virtual void endDrawScene() override;

protected:
    std::shared_ptr<rhi::IRenderer> _renderer{};
};
```

- [ ] **Step 2: 修改 `GLApp.cpp` 用 renderer 初始化**

`src/app/GL/GLApp.cpp`：
```cpp
#include "GLApp.hpp"
#include "Base/ErrorHandle.hpp"
#include "Base/Log.hpp"
#include "GL/GLHeader.hpp"          // 或 rhi/gl/GLHeader.hpp
#include "ImGuiOpenglWindow.hpp"
#include "rhi/gl/GLBackend.hpp"
#include "rhi/gl/GLFWSurface.hpp"
#include <imgui.h>

using namespace ErrorHandle;

GLApp::GLApp() {}
GLApp::~GLApp() {}

bool GLApp::initGraphics() {
    if (!m_window->initGLContext()) {
        LOGE("Failed to initialize GL Context");
        return false;
    }
    LOGI("OpenGL Vendor: {}", (char*)glGetString(GL_VENDOR));
    LOGI("OpenGL Renderer: {}", (char*)glGetString(GL_RENDERER));
    LOGI("OpenGL Version: {}", (char*)glGetString(GL_VERSION));

    auto props = m_window->getProperties();
    auto surface = std::make_shared<rhi::GLFWSurface>(
        m_window->getNativeGLFWWindow(), props.width, props.height);
    _renderer = rhi::createGLRenderer();
    if (!_renderer->init(surface)) {
        LOGE("Failed to init renderer");
        return false;
    }
    _renderer->setViewport(rhi::Viewport{0, 0, props.width, props.height});
    _renderer->setPipeline(nullptr);
    // 深度测试由各 App 的 pipeline 通过 setDepthTest 控制；默认打开一次
    m_imguiWindow = std::make_unique<ImGuiOpenglWindow>();
    m_imguiWindow->init(m_window->getNativeGLFWWindow());
    return true;
}

void GLApp::clearColor() {
    _renderer->clearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void GLApp::beginDrawScene() { return Application::beginDrawScene(); }

void GLApp::drawScene(const float dt) {
    ImGui::Begin("OpenGL");
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Hello Graphic! %.1f FPS", io.Framerate);
    ImGui::End();
    return Application::drawScene(dt);
}

void GLApp::endDrawScene() {
    _renderer->present();
    return Application::endDrawScene();
}
```
注意：`glGetString` 仍在 GLApp.cpp 用，违反"GL 限定 rhi/gl"。为满足约束，把版本打印移到 `GLBackend`（Task 9 处理）或暂时保留并注明后续清理。**计划采用**：把 `glGetString`/`glViewport`/`glEnable` 相关全部移入 rhi/gl（见 Task 9），GLApp.cpp 此处只保留 RHI 调用。上面片段中 `glGetString` 行应在 Task 9 移除。

- [ ] **Step 3: 编译验证（可能因 GL 头路径报错）**

```bash
cd /home/ares/workspace/GraphicsAPILearn
cmake -S . -B build -DENABLE_OPENGL=ON && cmake --build build -j 2>&1 | grep -E 'error:|GLApp'
```
预期：无 error（可能为 include 路径，需确认 `rhi/gl/GLHeader.hpp` 可被 `src/app` 引用，必要时调整 include 为相对 `src` 的路径）。

- [ ] **Step 4: Commit**

```bash
git add src/app/GL/GLApp.hpp src/app/GL/GLApp.cpp
git commit -m "refactor(app): GLApp drives rendering through IRenderer"
```

---

## App 迁移（GLXxxApp → XxxApp）

迁移模式固定，按批次进行。每个 App 迁移完成需**编译通过 + 运行验证**（截屏人工确认）。代表性完整示例见 Task 9（Triangle，端到端参考），后续批次的每个 App 按该模式与下方每 App 专属说明操作。

### Task 9: 迁移 Triangle（端到端参考）

**Files:**
- Create: `src/app/Base/TriangleApp.hpp` / `TriangleApp.cpp`
- Delete: `src/app/GL/Base/GLTriangleApp.hpp` / `GLTriangleApp.cpp`
- Modify: `src/app/GL/GLAppFactory.cpp`（去掉 Triangle 分支）
- Modify: `src/app/AppFactory.cpp`（或新注册表）

**Interfaces:**
- Consumes: `GLApp`（renderer()）、`rhi::IShader/IPipeline/IBuffer`、`Geometry/Triangle.hpp`、`StaticCollector`
- Produces: 第一个 API 无关的 `TriangleApp`，编译并运行

- [ ] **Step 1: 创建 `src/app/Base/TriangleApp.hpp`**

`src/app/Base/TriangleApp.hpp`：
```cpp
#pragma once
#include "App/GL/GLApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class TriangleApp : public GLApp {
public:
    virtual ~TriangleApp();
protected:
    virtual bool initApp() override;
    virtual void clearColor() override;
    virtual void beginDrawScene() override;
    virtual void drawScene(const float dt) override;
    virtual void endDrawScene() override;

private:
    std::shared_ptr<rhi::IShader> _shader{};
    std::shared_ptr<rhi::IPipeline> _pipeline{};
    std::shared_ptr<rhi::IBuffer> _vertexBuffer{};
};
```

- [ ] **Step 2: 创建 `src/app/Base/TriangleApp.cpp`**

`src/app/Base/TriangleApp.cpp`：
```cpp
#include "TriangleApp.hpp"
#include "Base/StaticCollector.hpp"
#include "Base/ErrorHandle.hpp"
#include "Geometry/Triangle.hpp"
#include <Utils/FileUtils.hpp>
#include <glm/glm.hpp>

using FileUtils::join;
using namespace ErrorHandle;

TriangleApp::~TriangleApp() {}

bool TriangleApp::initApp() {
    const auto vfile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.vert");
    const auto ffile = join(StaticCollector::getGLShaderPath(), "Base", "triangle.frag");

    _shader = renderer()->createShader();
    rhi::ShaderStage vs; vs.type = rhi::ShaderStage::Vertex; vs.source = vfile;
    rhi::ShaderStage fs; fs.type = rhi::ShaderStage::Fragment; fs.source = ffile;
    auto ok = _shader->compile({ vs, fs });
    ExitIfFailed(ok, "Failed to compile triangle shader: {}", _shader->getLog());

    rhi::VertexLayout layout;
    layout.elements = {
        { rhi::VertexElement::Float4, 0, 0, 32 },
        { rhi::VertexElement::Float4, 1, 16, 32 },
    };
    _pipeline = renderer()->createPipeline(layout, _shader);

    Triangle oneTriangle{};
    oneTriangle.toGL();
    _vertexBuffer = renderer()->createBuffer();
    _vertexBuffer->init(oneTriangle.data(), oneTriangle.byteSize(), rhi::BufferType::Vertex);
    return true;
}

void TriangleApp::clearColor() { return GLApp::clearColor(); }
void TriangleApp::beginDrawScene() { return GLApp::beginDrawScene(); }

void TriangleApp::drawScene(const float dt) {
    renderer()->setPipeline(_pipeline);
    renderer()->setVertexBuffer(_vertexBuffer);
    renderer()->draw(3);
    return GLApp::drawScene(dt);
}

void TriangleApp::endDrawScene() { return GLApp::endDrawScene(); }
```
注：`VertexElement` 的 `semantic`（location）需在 GLPipeline 绑定 vao 时映射 `glVertexAttribPointer`。当前 GLPipeline 未生成属性指针——需补：见 Task 9 Step 3 说明，或在 GLPipeline 增加 `applyLayout`。

- [ ] **Step 3: 补充 GLPipeline 顶点属性绑定（否则 vao 空，渲染无输出）**

在 `src/rhi/gl/GLPipeline.cpp` 的 `bindShader` 中，遍历 `_layout` 生成属性（使用 `VertexElement` 的 offset/stride）：
```cpp
bool GLPipeline::bindShader(const std::shared_ptr<GLShader>& shader, const VertexLayout& layout) {
    _shader = shader;
    _layout = layout.elements;
    if (_vao == 0) glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);
    for (size_t i = 0; i < _layout.size(); ++i) {
        const auto& e = _layout[i];
        int comps = (e.format == rhi::VertexElement::Float4) ? 4 :
                    (e.format == rhi::VertexElement::Float3) ? 3 : 2;
        glEnableVertexAttribArray(static_cast<GLuint>(e.semantic));
        glVertexAttribPointer(static_cast<GLuint>(e.semantic), comps, GL_FLOAT, GL_FALSE,
                              e.stride, reinterpret_cast<void*>(static_cast<uintptr_t>(e.offset)));
    }
    glBindVertexArray(0);
    return true;
}
```
`VertexElement` 已含 `offset`/`stride`（见 Task 1）。每个 App 的 layout 均须显式给出各元素 offset 与 stride（如 Triangle：stride=32，offset0=0，offset1=16）。

- [ ] **Step 4: 更新 AppFactory 使 main 默认选中 Triangle 验证**

修改 `src/app/main.cpp` 的 `auto type = AppType::Triangle;`（临时），构建运行确认窗口显示彩色三角形。

- [ ] **Step 5: 编译 + 运行验证**

```bash
cd /home/ares/workspace/GraphicsAPILearn
cmake -S . -B build -DENABLE_OPENGL=ON && cmake --build build -j
./build/renderLearn
```
预期：窗口显示红/绿/蓝三角形（与迁移前一致）。人工截屏对比确认。

- [ ] **Step 6: 更新 GLAppFactory 移除 Triangle 分支（改用 AppFactory 注册表，见 Task 10）**

- [ ] **Step 7: 删除旧文件，提交**

```bash
git rm src/app/GL/Base/GLTriangleApp.*
git add src/app/Base/TriangleApp.*
git commit -m "refactor(app): migrate TriangleApp to RHI"
```

---

### Task 10: 重构 AppFactory 为注册表/宏

**Files:**
- Create: `src/app/AppRegistry.hpp` / `AppRegistry.cpp`
- Modify: `src/app/AppFactory.cpp`

**Interfaces:**
- Consumes: 各 `XxxApp` 类
- Produces: `AppRegistry::create(AppType)` 用 `std::unordered_map<AppType, std::function<...>>` 替代 switch，App 迁移时一行注册即可

- [ ] **Step 1: 创建 `AppRegistry.hpp`**

`src/app/AppRegistry.hpp`：
```cpp
#pragma once
#include "App/AppType.hpp"
#include <memory>
#include <functional>

class IApplication;
class AppRegistry {
public:
    using Creator = std::function<std::shared_ptr<IApplication>()>;
    static void registerApp(AppType type, Creator creator);
    static std::shared_ptr<IApplication> create(AppType type);
};
```

- [ ] **Step 2: 创建 `AppRegistry.cpp`**

`src/app/AppRegistry.cpp`：
```cpp
#include "AppRegistry.hpp"
#include <unordered_map>

static std::unordered_map<AppType, AppRegistry::Creator>& Registry() {
    static std::unordered_map<AppType, AppRegistry::Creator> map;
    return map;
}

void AppRegistry::registerApp(AppType type, Creator creator) {
    Registry()[type] = std::move(creator);
}

std::shared_ptr<IApplication> AppRegistry::create(AppType type) {
    auto it = Registry().find(type);
    return it == Registry().end() ? nullptr : it->second();
}
```

- [ ] **Step 3: 修改 `AppFactory.cpp` 走注册表**

`src/app/AppFactory.cpp`：
```cpp
#include "AppFactory.hpp"
#include "AppRegistry.hpp"

std::shared_ptr<IApplication> AppFactory::create(const GraphicsType gtype, const AppType type) {
    if (gtype == GraphicsType::GL) {
        return AppRegistry::create(type);
    }
    // 其他 API 后端（DX11 等）后续阶段接入，暂返回 nullptr
    return nullptr;
}
```

- [ ] **Step 4: 提供注册函数（各 App 迁移时注册）**

新增 `src/app/RegisterApp.ipp`（或分散到各 App.cpp 内静态注册器）。推荐：在**每个迁移后的 App.cpp 顶部**放静态注册块：
```cpp
#include "AppRegistry.hpp"
namespace {
struct TriangleRegistrar { TriangleRegistrar() { AppRegistry::registerApp(AppType::Triangle, []{ return std::make_shared<TriangleApp>(); }); } };
const TriangleRegistrar g_triangleRegistrar{};
}
```
初始阶段需保证 `main.cpp` 引用的 App 已被注册。为最简：让 `AppFactory.cpp` 或一个 `AppRegister.cpp` include 所有已迁移 App 的 .cpp 注册块所在头，或用 `#include "TriangleApp.hpp"` + 手动注册。计划采用**静态注册器**方案（分散、自注册、无需中央维护）。

- [ ] **Step 5: 编译 + 运行验证**

```bash
cmake --build build -j && ./build/renderLearn
```
预期：Triangle 正常显示（与 Task 9 相同）。

- [ ] **Step 6: Commit**

```bash
git add src/app/AppRegistry.* src/app/AppFactory.cpp
git commit -m "refactor(app): replace factory switch with AppRegistry"
```

---

## 迁移批次（每个 App 一个子任务）

以下批次顺序执行。每批内**每个 App** 按 Task 9 的 Triangle 模式迁移：新建 `src/app/<dir>/<Name>App.{hpp,cpp}`（去 GL 前缀），继承 `GLApp`，用 `renderer()` 创建/使用资源，删除旧 `GL` 文件，更新工厂（现在通过静态注册器），编译运行验证，commit。

### Batch B1: Base 批（Triangle 已完成，其余 4 个）

#### Task 11: RectApp
- 旧：`src/app/GL/Base/GLRectApp.{hpp,cpp}` → 新：`src/app/Base/RectApp.{hpp,cpp}`
- 几何：`Geometry/Rect.hpp`（有 vao/vbo[2]/ebo，pos 4floats + uv 2floats）
- shader：`Base/SimpleTexture.vert/.frag` 或 `Rect.*`（照旧）
- 数据流：`createVertexBuffer()`（2 个 vbo：pos+uv，1 个 ebo）；`drawIndexed`（6 indices）。无纹理。
- layout：pos `Float4` semantic 0 offset0 stride24；uv `Float2` semantic 2 offset16 stride24。用两个 IBuffer（pos、uv）+ 一个 index buffer，`setVertexBuffer` 逐个 bind，`setIndexBuffer`，`drawIndexed(6)`。
- 注册 `AppType::Rect`。

#### Task 12: SimpleTextureApp
- 旧：`src/app/GL/Base/GLSimpleTextureApp.{hpp,cpp}` → 新：`src/app/Base/SimpleTextureApp.{hpp,cpp}`
- 纹理：`rhi::GLImageTexture2D`（从文件 `StaticCollector::getImagePath()/dog.jpg` 加载）
- 几何：`Rect`；shader `Base/SimpleTexture.vert/.frag`
- 数据流：pos+uv vbo、ebo、`drawIndexed(6)`；`renderer()->bindTexture(_texture, 0)`。uv shader 采样用 sampler2D。
- layout：pos `Float4` semantic0 offset0 stride24；uv `Float2` semantic2 offset16 stride24。
- 注册 `AppType::SimpleTexture`。

#### Task 13: CubeApp
- 旧：`src/app/GL/Base/GLCubeApp.{hpp,cpp}` → 新：`src/app/Base/CubeApp.{hpp,cpp}`
- 几何：`Geometry/Cube.hpp`；shader `Base/Cube.vert/.frag`
- 数据流：照 GL 版（pos/normal 一个 vbo + uv 一个 vbo + ebo），`drawIndexed(36)`。
- 注册 `AppType::Cube`。

#### Task 14: CameraApp（含 GLCameraBaseApp 基类）
- 旧：`src/app/GL/Base/GLCameraApp` + `GLCameraBaseApp` → 新：`src/app/Base/CameraApp` + `CameraBaseApp`
- 几何：`Geometry/Camera.hpp`（`_camera.zoom()`、`_camera.getViewMatrix()`）；投影 `glm::perspective`
- uniform：`projection`(mat4)、`view`(mat4)、`model`(mat4) 用 `setUniform(name, &m[0][0], 1)`
- 数据流：循环 1..count 个立方体，各自 model 矩阵；`renderer()->drawIndexed(36)` 每次。
- 注册 `AppType::Camera`。

### Batch B2: Light 批（SimpleLight* 系列，7 个 + LightSource 4 个）

这些 App 结构类似：几何 Cube/Plane/Sphere + shader + uniform 设灯光参数。**每个**按 Triangle 模式迁移，重点是把 `_program.update(name, ...)` 换成 `_pipeline->setUniform(...)` 的对应重载：
- `bool`/`int` → `setUniform(name, bool/int)`
- `float` → `setUniform(name, float)`
- `glm::vec3/vec4` → `setUniform(name, &v[0], 1, 3/4)`
- `glm::mat3/mat4` → `setUniform(name, &m[0][0], 1)`
- `std::vector<glm::vec3/vec4>` → `setUniform(name, &v[0][0], count, 3/4)`

#### Task 15: SimpleLightAmbination（AppType::SimpleLight_Ambination）
#### Task 16: SimpleLightDiffuse（AppType::SimpleLight_Diffuse）
#### Task 17: SimpleLightSpecular（AppType::SimpleLight_Specular）
#### Task 18: SimpleLightMaterial（AppType::SimpleLight_Material）
#### Task 19: SimpleLightMap（AppType::SimpleLight_Map）
#### Task 20: GLLightSourceDirection → LightSourceDirection（AppType::SimpleLight_Source_Direction）
#### Task 21: GLLightSourcePoint → LightSourcePoint（AppType::SimpleLight_Source_Point）
#### Task 22: GLLightSourceSpot → LightSourceSpot（AppType::SimpleLight_Source_Spot）
#### Task 23: GLLightSourceMult → LightSourceMult（AppType::SimpleLight_Source_Mult）

### Batch B3: Model 批

#### Task 24: LoadModelApp（AppType::LoadModel）
- 旧：`src/app/GL/Model/GLLoadModelApp` → 新：`src/app/Model/LoadModelApp`
- 使用 `Model/Model.hpp`、`Model/Mesh.hpp`、assimp；每个 mesh 有 vao/vbo/ebo/纹理。迁移时把 GL 相关（`GLImageTexture2D`、`glGen*`）替换为 RHI（`createBuffer/createTexture2D/bindTexture`）。Mesh 内部的 GL 资源引用改为 `std::shared_ptr<rhi::IBuffer>`/`rhi::ITexture2D`。**此项改动涉及 `src/model/Mesh.hpp`**：把其 GL 成员替换为 RHI 接口指针，并在 init 时用 `renderer` 创建。这是对 model 层的侵入性修改——需谨慎，可能此 App 迁移时 model 层同步改造。

### Batch B4: Advanced 批（12 个）

每个按 Triangle 模式 + 各自特性。涉及 `IRenderTarget`（FrameBuffer）、状态设置（Blend/CullFace/DepthTest 用 `pipeline->set*`）、uniform buffer（UniformBuffer 用 `_pipeline->setUniform` + uniformBind）、几何着色器（Explode/AdvancedGLSL 用 `IShader::compile` 传 3 个 stage）。

#### Task 25: DepthTestApp（AppType::DepthTest）
#### Task 26: TemplateTestApp（AppType::TemplateTest）——模板测试需额外状态（glStencilFunc/glStencilOp），若 IPipeline 无此接口，本 App 暂在 rhi/gl 增加 `setStencilTest` 扩展或在 App 内（违反约束）——计划：在 `IPipeline` 增加 `setStencilTest(bool)`、`setStencilFunc(func, ref, mask)`、`setStencilOp(...)` 接口并实现，App 用 RHI 调用。
#### Task 27: BlendApp（AppType::Blend）——`pipeline->setBlend(true)`
#### Task 28: CullFaceApp（AppType::CullFace）——`pipeline->setCullMode(true, face)`
#### Task 29: FrameBufferApp（AppType::FrameBuffer）——`IRenderTarget`（FBO + 颜色纹理 + RBO），后处理屏幕三角形用 `bindTexture(_screenRT->colorTexture())`
#### Task 30: SkyBoxApp（AppType::SkyBox）——立方体贴图纹理 `GLTexture3D`
#### Task 31: AdvancedShaderApp（AppType::AdvancedShader）——几何着色器 3 stage compile
#### Task 32: UniformBufferApp（AppType::UniformBuffer）——`glUniformBlockBinding` 迁移，需 `IPipeline` 增加 `setUniformBufferBinding(name, binding)`；App 用 RHI
#### Task 33: SimpleGemoteryApp（AppType::SimpleGeometry）
#### Task 34: ExplodeApp（AppType::Explode）——几何着色器
#### Task 35: NormalLineApp（AppType::NormalLine）
#### Task 36: MultieInstanceApp（AppType::MultiInstance）——instance：`glDrawArraysInstanced`。**计划**：在 `IRenderer` 增加 `drawInstanced(vertexCount, instanceCount)`，GL 后端实现 `glDrawArraysInstanced`。
#### Task 37: SaturnApp（AppType::MultiInstance_Saturn）
#### Task 38: MsaaApp（AppType::Msaa）——`glEnable(GL_MULTISAMPLE)`；`IPipeline::setMultisample(bool)`。

### Batch B5: Light/Advanced 批（10 个）

#### Task 39: BlinnPhongApp（AppType::BlinnPhong）
#### Task 40: GammaApp（AppType::Gamma）
#### Task 41: ShadowMapApp（AppType::Shadow_Map）——深度渲染目标 `IRenderTarget`
#### Task 42: ShadowApp（AppType::Shadow）
#### Task 43: PointLightShadowApp（AppType::Shadow_PointLight）——立方体深度贴图 `GLTexture3D`
#### Task 44: NormalMapApp（AppType::NormalMap）
#### Task 45: ParallaxMapApp（AppType::ParallaxMap）
#### Task 46: HdrApp（AppType::Hdr）——HDR FBO `IRenderTarget`（16F 颜色纹理）+ tonemap 屏幕三角形
#### Task 47: BloomApp（AppType::Bloom）——多 pass FBO + ping-pong 渲染目标
#### Task 48: DeferApp（AppType::Defer）——GBuffer 多渲染目标 `IRenderTarget`
#### Task 49: SSAOApp（AppType::SSAO）——多 pass + 噪声纹理

### Batch B6: PBR 批（5 个）

#### Task 50: PBRBaseApp（AppType::PBR_Base）
#### Task 51: PBRTextureApp（AppType::PBR_Texture）
#### Task 52: IBLIrradianceConversionApp（AppType::PBR_IBL_Irradiance_Conversion）
#### Task 53: IBLIrradianceApp（AppType::PBR_IBL_Irradiance）
#### Task 54: IBLSpecularApp（AppType::PBR_IBL_Specular）

---

## 清理收尾

### Task 55: 迁移完成后清理 native/GL 冗余与 TODO.md

**Files:**
- Delete: `src/native/GL/GLProgram.*`、`GLCube.*`、`GLPlane.*`、`GLSphere.*`（若已无引用）
- Modify: `src/native/ITexture2D.hpp`（若 native 纹理不再被 App 使用，迁移到 rhi/core 后从 native 移除或保留为兼容）
- Modify: `TODO.md`（更新 OpenGL 完成状态）
- Modify: `src/app/main.cpp`（恢复为最终选中的 App 或保留某个，注释更新）

- [ ] **Step 1: 全局搜索确认无残留 GL 直接引用**

```bash
cd /home/ares/workspace/GraphicsAPILearn
rg -l 'glad/glad.h|gl[A-Z]' src/app src/base src/geometry src/utils src/model src/native --glob '!rhi/**' | head -50
```
逐个处理仍含 GL 引用的文件（迁移或删除）。**验收标准**：除 `src/rhi/gl/` 外，项目源码无 `gl*` 调用、无 `GL_` 宏、无 `glad/glad.h` include。

- [ ] **Step 2: 全量构建 + 全部 App 回归运行**

```bash
cmake --build build -j
```
对每个 AppType 依次设置 `main.cpp` 运行，人工确认渲染正常。记录异常。

- [ ] **Step 3: 更新 TODO.md 与文档**

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: clean up native GL leftovers and update docs after RHI migration"
```

---

## Self-Review（执行后由计划作者复核）

- **Spec 覆盖**：设计文档的目标（Linux+GL 全量迁移、GL 限定 rhi/gl、App 去 GL 前缀、工厂去 switch）均映射到 Task 0-55。
- **占位符检查**：迁移批次的 App（Task 11-54）以"按 Triangle 模式 + 专属说明"给出，未重复完整代码（量极大），但每个 App 给出了：旧→新路径、几何、shader、uniform 重载映射、layout、注册 AppType、所需接口扩展（stencil/instance/multisample/uniform-block）。执行者需对照旧文件精确迁移。
- **类型一致性**：接口命名在 Task 1 统一（`setUniform` 5 重载、`drawIndexed`、`drawInstanced` 待加），App 与后端均使用 `rhi::` 命名空间。Task 9 修正 `VertexElement` 增加 offset/stride 需回填到 Task 1。

## 执行交接

计划已保存至 `docs/superpowers/plans/2026-08-08-rhi-architecture.md`。

**两种执行方式：**

1. **Subagent-Driven（推荐）**——每个任务派发独立 subagent，任务间双阶段评审，迭代快。
2. **Inline Execution**——本会话内用 executing-plans 批量执行，带检查点。

选择哪种？
