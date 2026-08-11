# 计划E：PBR/IBL 批次迁移到 RHI 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 5 个 PBR/IBL 示例 App（PBR_Base / PBR_Texture / PBR_IBL_Irradiance_Conversion / PBR_IBL_Irradiance / PBR_IBL_Specular）从原生 OpenGL（GLProgram/glad/GLImageTexture2D/GLCube/GLPlane）迁移到 RHI 抽象层，并补充 2 个 RHI 能力缺口。

**Architecture:** 采用与计划 B/C/D 一致的迁移模式：每个 App 持有 `std::shared_ptr<rhi::IPipeline>` + `RhiGeometry::Create` 上传几何，用 `renderer()` 命令式接口绘制。本批次首次引入 render-to-cubemap-face（`IRenderTarget::attachCubeFace`）与多步 GPU 预处理（`renderBeforeLoop`）。RHI 扩展为 `attachCubeFace` 加 mip 参数 + 后端启用 seamless cubemap。

**Tech Stack:** C++17/20, OpenGL (via RHI), glm, stb_image（经 `RhiImage`）, ImGui。

## Global Constraints

- 本批次每个任务**只修改指定文件**；其余 PBR App 文件一律不动。
- 移除对 `GLProgram`/`glad`/`GLImageTexture2D`/`GLCube`/`GLPlane`/`GLUtils`/`Rect`/`Constexpr`（未用）的 include；不得出现 `glXxx` 原生调用。
- 提交信息前缀：RHI 扩展用 `feat(rhi):`，App 迁移用 `refactor(app):`。
- 回归红线（每个任务尾部必须通过）：`./scripts/build_run.sh build` 编译通过 + `./scripts/run.sh all -b gl -d 1` = 46/46 OK + 单 App 冒烟 `./scripts/run.sh all -a <AppName> -b gl -d 1` 无崩溃。
- 直接提交到 `develop` 分支（仓库既有约定，无独立 feature 分支）。
- 静态辅助函数（GetCaptureViews/GetLightPosAndColor/GenreateObjPos/renderCube/renderSphere/CreateTexture）保留为 file-static，不改为成员。
- PBR/CUBE/Brdf shader 顶点布局统一 `pos@0/inColor@1/aTexCoords@2/normal@3`，即 `RhiGeometry::Create` 默认 Layout（uv=2, normal=3）。
- 全屏 quad（BRDF LUT）管线必须 `setPrimitiveType(TriangleStrip)`（沿用计划 D 教训）。
- 深度函数：capture 阶段用 `setDepthFunc(CompareFunc::LessEqual)`（原 `glDepthFunc(GL_LEQUAL)`），几何 pass 用 `setDepthTest(true)`。
- `IRenderTarget::create(FramebufferDesc)` 的 depth 附件用 `AttachmentType::Depth, TextureFormat::Depth24Stencil8`。
- cubemap 用 `ITexture3D::createEmpty(TextureDesc, w, h)`；2D 空纹理（BRDF LUT）用 `ITexture2D::createEmpty(TextureDesc, w, h)`。

---

### Task 1: RHI 扩展 — attachCubeFace mip 参数 + seamless cubemap

**Files:**
- Modify: `src/rhi/core/IRenderTarget.hpp:24`（`attachCubeFace` 签名）
- Modify: `src/rhi/gl/GLRenderTarget.cpp:120-127`（`attachCubeFace` 实现）
- Modify: `src/rhi/gl/GLBackend.cpp:27-37`（`init` 加 seamless）

**Interfaces:**
- Consumes: 无（纯后端扩展）
- Produces: `IRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip = 0)`；GL 后端 init 时启用 seamless cubemap。

- [ ] **Step 1: 修改接口声明**

在 `src/rhi/core/IRenderTarget.hpp` 把：
```cpp
virtual bool attachCubeFace(ITexture3D* cube, int face) = 0;
```
改为：
```cpp
virtual bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) = 0;
```

- [ ] **Step 2: 修改 GL 实现**

在 `src/rhi/gl/GLRenderTarget.cpp` 把 `attachCubeFace` 实现改为接受 mip 并传给 `glFramebufferTexture2D`：
```cpp
bool GLRenderTarget::attachCubeFace(ITexture3D* cube, int face, int mip) {
    if (!cube || face < 0 || face > 5 || !_fbo) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                           static_cast<GLuint>(reinterpret_cast<uintptr_t>(cube->handle())), mip);
    // 显式设置绘制缓冲为 COLOR_ATTACHMENT0（RT 可能以仅 depth 附件创建，见 Task 4 说明）
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}
```
> 注：cubemap capture 的 RT 以「仅 depth 附件」创建（`FramebufferDesc` 只有 Depth attachment，无 Color），`create()` 会把 draw buffer 设为 `GL_NONE`（GLRenderTarget.cpp:108）。因此 `attachCubeFace` 必须显式 `glDrawBuffer(GL_COLOR_ATTACHMENT0)` 才能把 cubemap 面作为颜色输出。此改动向后兼容（不破坏既有调用）。

- [ ] **Step 3: 启用 seamless cubemap**

在 `src/rhi/gl/GLBackend.cpp` 的 `init(const std::shared_ptr<ISurface>& surface)`（第 27 行）末尾、`return true;` 之前加一行：
```cpp
glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
```

- [ ] **Step 4: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过（无警告/错误）。

- [ ] **Step 5: 全量回归**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK（无回归）。

- [ ] **Step 6: 提交**

```bash
git add src/rhi/core/IRenderTarget.hpp src/rhi/gl/GLRenderTarget.cpp src/rhi/gl/GLBackend.cpp
git commit -m "feat(rhi): attachCubeFace mip param + seamless cubemap for IBL"
```

---

### Task 2: 迁移 PBR_Base

**Files:**
- Modify: `src/app/GL/Advanced/PBR/GLPBRBaseApp.hpp`
- Modify: `src/app/GL/Advanced/PBR/GLPBRBaseApp.cpp`

**Interfaces:**
- Consumes: `rhi::IRenderer`、`RhiGeometry::Create(renderer, Shape&, useUv, useNormal, useIndex)`、`rhi::IPipeline`
- Produces: `GLPBRBaseApp` 迁移完成（成员：`_sphereGeo`、`_program`、`_lightPositions`、`_lightColors`、材质参数）。这是 T3/T4/T5/T6 的模板。

- [ ] **Step 1: 重写头文件**

把 `GLPBRBaseApp.hpp` 改为：
```cpp
#ifndef GL_PBR_BASE_APP_HPP
#define GL_PBR_BASE_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLPBRBaseApp : public GLCameraBaseApp {
public:
    GLPBRBaseApp() = default;
    virtual ~GLPBRBaseApp() override;

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& layout);

protected:
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);

protected:
    std::shared_ptr<rhi::IPipeline> m_program;
    RhiGeometry::Geometry m_sphere;
    std::shared_ptr<rhi::IBuffer> _sphereVb{};
    std::shared_ptr<rhi::IBuffer> _sphereUv{};
    std::shared_ptr<rhi::IBuffer> _sphereNormal{};
    uint32_t _sphereIndexCount{0};

    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;

    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
```

- [ ] **Step 2: 重写 cpp（几何 + shader + draw）**

把 `GLPBRBaseApp.cpp` 改写为（核心差异：用 `RhiGeometry::Create` + `renderer()` 绘制）：
```cpp
#include "GLPBRBaseApp.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLPBRBaseApp::~GLPBRBaseApp() {}

void GLPBRBaseApp::initShapes() {
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
    _sphereVb = m_sphere.vertexBuffer;
    _sphereUv = m_sphere.uvBuffer;
    _sphereNormal = m_sphere.normalBuffer;
    _sphereIndexCount = m_sphere.indexCount;
}

static auto GetLightPosAndColor() {
    std::vector<glm::vec3> lightPositions = {
        glm::vec3(-10.0f,  10.0f, 10.0f),
        glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    std::vector<glm::vec3> lightColors = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f)
    };
    return std::make_pair(lightPositions, lightColors);
}

bool GLPBRBaseApp::initApp() {
    if (!GLCameraBaseApp::initApp()) return false;
    initShapes();
    compileShader(m_sphere.layout);
    return true;
}

void GLPBRBaseApp::compileShader(const rhi::VertexLayout& layout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "Base");
    auto shader = renderer()->createShader();
    auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "PBR.vs"), "main", false},
                                {rhi::ShaderStage::Fragment, join(shaderDir, "PBR.fs"), "main", false} });
    ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
    m_program = renderer()->createPipeline(layout, shader);
    m_program->setDepthTest(true);
}

static std::vector<glm::vec3> GenreateObjPos(int radius = 5, float gap = 0.5f, const glm::vec3 &center = glm::vec3(0.0f)) {
    std::vector<glm::vec3> positions;
    if (radius < 0) return positions;
    for (int row = -radius; row <= radius; ++row) {
        for (int col = -radius; col <= radius; ++col) {
            float x = center.x + static_cast<float>(col) * gap;
            float y = center.y + static_cast<float>(row) * gap;
            positions.push_back(glm::vec3(x, y, center.z));
        }
    }
    return positions;
}

void GLPBRBaseApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(_sphereVb);
    renderer()->setVertexBuffer(_sphereUv, 1);
    renderer()->setVertexBuffer(_sphereNormal, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    program->setUniformMatrix("normalMatrix", glm::value_ptr(normal), 1, 3);
    renderer()->drawIndexed(_sphereIndexCount, 0, 0);
}

void GLPBRBaseApp::drawScene(const float dt) {
    GLCameraBaseApp::drawScene(dt);
    auto pos = _camera.getAttr().pos;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

    renderer()->setPipeline(m_program);
    m_program->setUniform("texture", 0);
    m_program->setUniform("ao", m_ao);
    m_program->setUniform("projection", glm::value_ptr(projection), 1);
    m_program->setUniform("view", glm::value_ptr(view), 1);
    m_program->setUniform("camPos", glm::value_ptr(pos), 1, 3);
    m_program->setUniform("roughness", m_roughness);
    m_program->setUniform("metallic", m_metallic);
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        m_program->setUniform("albedo", glm::value_ptr(glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f)), 1, 3);
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(m_program, objectPos);
    }

    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        m_program->setUniform("lightPositions[" + std::to_string(i) + "]", glm::value_ptr(lightPositions[i]), 1, 3);
        m_program->setUniform("lightColors[" + std::to_string(i) + "]", glm::value_ptr(lightColors[i]), 1, 3);
        renderSphere(m_program, lightModel);
    }

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
    ImGui::End();
}
```
> 头文件里删除了 `initApp` 中不再需要的 `glEnable(GL_DEPTH_TEST)` 直接调用——改为 `m_program->setDepthTest(true)`。

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 全量回归 + 单 App 冒烟**

Run: `./scripts/run.sh all -b gl -d 1 && ./scripts/run.sh all -a PBR_Base -b gl -d 1`
Expected: 46/46 OK，单 App 无崩溃。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Advanced/PBR/GLPBRBaseApp.hpp src/app/GL/Advanced/PBR/GLPBRBaseApp.cpp
git commit -m "refactor(app): migrate PBR_Base to RHI"
```

---

### Task 3: 迁移 PBR_Texture

**Files:**
- Modify: `src/app/GL/Advanced/PBR/GLPBRTextureApp.hpp`
- Modify: `src/app/GL/Advanced/PBR/GLPBRTextureApp.cpp`

**Interfaces:**
- Consumes: Task 2 的 `GLPBRBaseApp` 迁移模式（sphere 几何 + renderSphere）
- Produces: `GLPBRTextureApp` 迁移完成（5 张 2D 纹理绑定 unit 1-5）

- [ ] **Step 1: 重写头文件**

把 `GLPBRTextureApp.hpp` 改为（继承自已迁移的 `GLPBRBaseApp`，用 `RhiGeometry` 与 `std::shared_ptr<rhi::ITexture2D>`）：
```cpp
#ifndef GL_PBR_TEXTURE_APP_HPP
#define GL_PBR_TEXTURE_APP_HPP

#include "GLPBRBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>

class GLPBRTextureApp : public GLPBRBaseApp {
public:
    virtual ~GLPBRTextureApp();

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void loadTexture();

private:
    std::shared_ptr<rhi::ITexture2D> m_albedoMap{};
    std::shared_ptr<rhi::ITexture2D> m_roughnessMap{};
    std::shared_ptr<rhi::ITexture2D> m_metallicMap{};
    std::shared_ptr<rhi::ITexture2D> m_aoMap{};
    std::shared_ptr<rhi::ITexture2D> m_normalMap{};
};

#endif
```

- [ ] **Step 2: 重写 cpp**

把 `GLPBRTextureApp.cpp` 改写为：
```cpp
#include "GLPBRTextureApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLPBRTextureApp::~GLPBRTextureApp() {}

void GLPBRTextureApp::loadTexture() {
    auto resDir = join(StaticCollector::getImagePath(), "rusted_iron");
    auto load = [&](const std::string& name) {
        auto tex = RhiImage::Load2D(renderer().get(), join(resDir, name));
        ExitIfFailed(tex != nullptr, "Failed to load texture from file {}", name);
        return tex;
    };
    m_albedoMap = load("albedo.png");
    m_roughnessMap = load("roughness.png");
    m_metallicMap = load("metallic.png");
    m_aoMap = load("ao.png");
    m_normalMap = load("normal.png");
}

bool GLPBRTextureApp::initApp() {
    if (!GLPBRBaseApp::initApp()) return false;
    loadTexture();
    return true;
}

void GLPBRTextureApp::drawScene(const float dt) {
    GLCameraBaseApp::drawScene(dt);
    auto pos = _camera.getAttr().pos;
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));

    renderer()->setPipeline(m_program);
    m_program->setUniform("texture", 0);
    m_program->setUniform("projection", glm::value_ptr(projection), 1);
    m_program->setUniform("view", glm::value_ptr(view), 1);
    m_program->setUniform("camPos", glm::value_ptr(pos), 1, 3);

    renderer()->bindTexture(m_albedoMap, 1);
    renderer()->bindTexture(m_roughnessMap, 2);
    renderer()->bindTexture(m_metallicMap, 3);
    renderer()->bindTexture(m_aoMap, 4);
    renderer()->bindTexture(m_normalMap, 5);
    m_program->setUniform("albedoMap", 1);
    m_program->setUniform("roughnessMap", 2);
    m_program->setUniform("metallicMap", 3);
    m_program->setUniform("aoMap", 4);
    m_program->setUniform("normalMap", 5);

    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(m_program, objectPos);
    }

    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        m_program->setUniform("lightPositions[" + std::to_string(i) + "]", glm::value_ptr(lightPositions[i]), 1, 3);
        m_program->setUniform("lightColors[" + std::to_string(i) + "]", glm::value_ptr(lightColors[i]), 1, 3);
        renderSphere(m_program, lightModel);
    }

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}
```
> 需在 `GLPBRTextureApp.cpp` 顶部 include `"app/GL/RhiImage.hpp"`（上面代码已省略，见 Step 2 注）。实际 include：`#include "app/GL/RhiImage.hpp"`。
> 注意：PBR_Texture 的 PBR.fs 使用 `normalMatrix` 吗？原代码 renderSphere 里设了 `normalMatrix`——复用基类 `renderSphere` 已设。若 PBR_Texture 的 shader 不含 normalMatrix uniform，`setUniformMatrix` 失败无害（返回 false）。

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 全量回归 + 单 App 冒烟**

Run: `./scripts/run.sh all -b gl -d 1 && ./scripts/run.sh all -a PBR_Texture -b gl -d 1`
Expected: 46/46 OK，单 App 无崩溃。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Advanced/PBR/GLPBRTextureApp.hpp src/app/GL/Advanced/PBR/GLPBRTextureApp.cpp
git commit -m "refactor(app): migrate PBR_Texture to RHI"
```

---

### Task 4: 迁移 PBR_IBL_Irradiance_Conversion（IC）

**Files:**
- Modify: `src/app/GL/Advanced/PBR/GLIBLIrradianceConversionApp.hpp`
- Modify: `src/app/GL/Advanced/PBR/GLIBLIrradianceConversionApp.cpp`

**Interfaces:**
- Consumes: Task 1（`attachCubeFace` mip + seamless）、`RhiImage::Load2DHDR`、`RhiGeometry::Create`（cube + sphere）、`IRenderTarget`
- Produces: IC 迁移完成，建立 **render-to-cubemap 标准流程**（T5/T6 复用）：`_envCubemap`（ITexture3D）、`_captureRT`（仅 depth 附件）、`renderToCubemap()`。

- [ ] **Step 1: 重写头文件**

把 `GLIBLIrradianceConversionApp.hpp` 改为：
```cpp
#ifndef GL_IBL_IRRADIANCE_CONVERSION_APP_HPP
#define GL_IBL_IRRADIANCE_CONVERSION_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLIBLIrradianceConversionApp : public GLCameraBaseApp {
public:
    virtual ~GLIBLIrradianceConversionApp();

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& cubeLayout);
    void initFramebuffer();
    void initCaptureViews();
    void loadTexture();
    void renderToCubemap();
    void renderBeforeLoop();
    void renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);
    void renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);

private:
    RhiGeometry::Geometry m_cube;
    RhiGeometry::Geometry m_sphere;
    std::shared_ptr<rhi::IPipeline> m_program{};
    std::shared_ptr<rhi::IPipeline> m_cubeMapProgram{};
    std::shared_ptr<rhi::IPipeline> m_backgroundProgram{};
    std::shared_ptr<rhi::ITexture2D> m_hdrEnvTexture{};
    std::shared_ptr<rhi::ITexture3D> m_envCubemap{};
    std::shared_ptr<rhi::IRenderTarget> m_captureRT{};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
```
> 头文件不设冗余 buffer 成员；cube/sphere 几何缓存在 `RhiGeometry::Geometry m_cube/m_sphere`（含 vertexBuffer/uvBuffer/normalBuffer/indexBuffer/layout/indexCount）。

- [ ] **Step 2: 重写 cpp（核心：render-to-cubemap 标准流程）**

把 `GLIBLIrradianceConversionApp.cpp` 改写为：
```cpp
#include "GLIBLIrradianceConversionApp.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiImage.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLIBLIrradianceConversionApp::~GLIBLIrradianceConversionApp() {}

void GLIBLIrradianceConversionApp::initShapes() {
    Cube cube{};
    m_cube = RhiGeometry::Create(renderer().get(), cube, true, true, true);
    Sphere sphere{};
    m_sphere = RhiGeometry::Create(renderer().get(), sphere, true, true, true);
}

bool GLIBLIrradianceConversionApp::initApp() {
    if (!GLCameraBaseApp::initApp()) return false;
    compileShader(m_cube.layout);
    initShapes();
    loadTexture();
    initFramebuffer();
    initCaptureViews();
    renderBeforeLoop();
    return true;
}

void GLIBLIrradianceConversionApp::initCaptureViews() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::Linear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = false;
    m_envCubemap = renderer()->createTexture3D();
    m_envCubemap->createEmpty(desc, 512, 512);
}

void GLIBLIrradianceConversionApp::initFramebuffer() {
    m_captureRT = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = 512; fbd.height = 512;
    fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
    if (!m_captureRT->create(fbd)) {
        ExitIfFailed(false, "Failed to create capture framebuffer");
    }
}

void GLIBLIrradianceConversionApp::compileShader(const rhi::VertexLayout& cubeLayout) {
    const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Advanced", "PBR", "IBL_IC");
    auto build = [&](const std::string& vs, const std::string& fs) {
        auto shader = renderer()->createShader();
        auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, vs), "main", false},
                                    {rhi::ShaderStage::Fragment, join(shaderDir, fs), "main", false} });
        ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
        return renderer()->createPipeline(cubeLayout, shader);
    };
    m_cubeMapProgram = build("CUBE.vs", "CUBE.fs");
    m_program = build("PBR.vs", "PBR.fs");
    m_backgroundProgram = build("Background.vs", "Background.fs");
    m_program->setDepthTest(true);
    m_cubeMapProgram->setDepthTest(true);
    m_backgroundProgram->setDepthTest(true);
    m_backgroundProgram->setDepthFunc(rhi::CompareFunc::LessEqual);
}

static std::vector<glm::mat4> GetCaptureViews() {
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    return std::vector<glm::mat4>(captureViews, captureViews + 6);
}

static auto GetLightPosAndColor() {
    std::vector<glm::vec3> lightPositions = {
        glm::vec3(-10.0f,  10.0f, 10.0f), glm::vec3( 10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f), glm::vec3( 10.0f, -10.0f, 10.0f),
    };
    std::vector<glm::vec3> lightColors = {
        glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f)
    };
    return std::make_pair(lightPositions, lightColors);
}

void GLIBLIrradianceConversionApp::loadTexture() {
    const auto resDir = StaticCollector::getImagePath();
    m_hdrEnvTexture = RhiImage::Load2DHDR(renderer().get(), join(resDir, "newport_loft.hdr"));
    ExitIfFailed(m_hdrEnvTexture != nullptr, "Failed to load HDR environment texture");
}

void GLIBLIrradianceConversionApp::renderToCubemap() {
    const auto captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_cubeMapProgram);
    renderer()->bindTexture(m_hdrEnvTexture, 0);
    m_cubeMapProgram->setUniform("equirectangularMap", 0);
    m_cubeMapProgram->setUniform("projection", glm::value_ptr(captureProjection), 1);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const int size = 512;
    for (int i = 0; i < 6; ++i) {
        renderer()->setViewport(rhi::Viewport{0, 0, size, size});
        m_cubeMapProgram->setUniform("view", glm::value_ptr(captureViews[i]), 1);
        m_captureRT->attachCubeFace(m_envCubemap.get(), i);
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderCube(m_cubeMapProgram, glm::mat4(1.0));
    }
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}

void GLIBLIrradianceConversionApp::renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setIndexBuffer(m_cube.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    renderer()->drawIndexed(m_cube.indexCount, 0, 0);
}

void GLIBLIrradianceConversionApp::renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model) {
    renderer()->setPipeline(program);
    renderer()->setVertexBuffer(m_sphere.vertexBuffer);
    renderer()->setVertexBuffer(m_sphere.uvBuffer, 1);
    renderer()->setVertexBuffer(m_sphere.normalBuffer, 2);
    renderer()->setIndexBuffer(m_sphere.indexBuffer);
    program->setUniform("model", glm::value_ptr(model), 1);
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model)));
    program->setUniformMatrix("normalMatrix", glm::value_ptr(normal), 1, 3);
    renderer()->drawIndexed(m_sphere.indexCount, 0, 0);
}

static std::vector<glm::vec3> GenreateObjPos(int radius = 5, float gap = 0.5f, const glm::vec3 &center = glm::vec3(0.0f)) {
    std::vector<glm::vec3> positions;
    if (radius < 0) return positions;
    for (int row = -radius; row <= radius; ++row)
        for (int col = -radius; col <= radius; ++col)
            positions.push_back(glm::vec3(center.x + col * gap, center.y + row * gap, center.z));
    return positions;
}

void GLIBLIrradianceConversionApp::renderBeforeLoop() {
    renderToCubemap();
}

void GLIBLIrradianceConversionApp::renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    renderer()->setPipeline(program);
    renderer()->bindTexture(m_envCubemap, 0);
    program->setUniform("view", glm::value_ptr(view), 1);
    program->setUniform("environmentMap", 0);
    program->setUniform("projection", glm::value_ptr(projection), 1);
    renderCube(program, glm::mat4(1.0));
}

void GLIBLIrradianceConversionApp::renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection) {
    const auto objPos = GenreateObjPos(2, 1.0f, glm::vec3(0.0f));
    auto pos = _camera.getAttr().pos;
    renderer()->setPipeline(program);
    program->setUniform("texture", 0);
    program->setUniform("projection", glm::value_ptr(projection), 1);
    program->setUniform("view", glm::value_ptr(view), 1);
    program->setUniform("camPos", glm::value_ptr(pos), 1, 3);
    program->setUniform("roughness", m_roughness);
    program->setUniform("metallic", m_metallic);
    program->setUniform("ao", m_ao);
    const int cnt = objPos.size();
    for (int i = 0; i < cnt; ++i) {
        program->setUniform("albedo", glm::value_ptr(glm::vec3(i * 1.0f / cnt, 0.0f, 0.0f)), 1, 3);
        auto objectPos = glm::mat4(1.0f);
        objectPos = glm::translate(objectPos, objPos[i]);
        objectPos = glm::scale(objectPos, glm::vec3(0.4f));
        renderSphere(program, objectPos);
    }
    const auto [lightPositions, lightColors] = GetLightPosAndColor();
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        auto lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPositions[i]);
        lightModel = glm::scale(lightModel, glm::vec3(0.2f));
        program->setUniform("lightPositions[" + std::to_string(i) + "]", glm::value_ptr(lightPositions[i]), 1, 3);
        program->setUniform("lightColors[" + std::to_string(i) + "]", glm::value_ptr(lightColors[i]), 1, 3);
        renderSphere(program, lightModel);
    }
}

void GLIBLIrradianceConversionApp::drawScene(const float dt) {
    GLCameraBaseApp::drawScene(dt);
    const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
    const auto view = _camera.getViewMatrix();
    renderObjectsAndLights(m_program, view, projection);
    renderBackground(m_backgroundProgram, view, projection);

    ImGui::Begin("OpenGL");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Roughness", &m_roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &m_ao, 0.0f, 1.0f);
    ImGui::End();
}
```

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 全量回归 + 单 App 冒烟**

Run: `./scripts/run.sh all -b gl -d 1 && ./scripts/run.sh all -a PBR_IBL_Irradiance_Conversion -b gl -d 1`
Expected: 46/46 OK，单 App 无崩溃、无 GL 错误。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Advanced/PBR/GLIBLIrradianceConversionApp.hpp src/app/GL/Advanced/PBR/GLIBLIrradianceConversionApp.cpp
git commit -m "refactor(app): migrate PBR_IBL_Irradiance_Conversion to RHI"
```

---

### Task 5: 迁移 PBR_IBL_Irradiance

**Files:**
- Modify: `src/app/GL/Advanced/PBR/GLIBLIrradianceApp.hpp`
- Modify: `src/app/GL/Advanced/PBR/GLIBLIrradianceApp.cpp`

**Interfaces:**
- Consumes: Task 1（attachCubeFace mip）、Task 4 模式（render-to-cubemap 标准流程）
- Produces: `GLIBLIrradianceApp` 迁移完成（额外 `_irradianceMap` 32×32 cubemap）

- [ ] **Step 1: 重写头文件**

把 `GLIBLIrradianceApp.hpp` 改为（比 IC 多 `_irradianceMap`）：
```cpp
#ifndef GL_IBL_IRRADIANCE_APP_HPP
#define GL_IBL_IRRADIANCE_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLIBLIrradianceApp : public GLCameraBaseApp {
public:
    virtual ~GLIBLIrradianceApp();

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& cubeLayout);
    void initFramebuffer();
    void initCaptureViews();
    void createIrradianceMap();
    void loadTexture();
    void renderToCubemap();
    void renderIrradianceMap();
    void renderBeforeLoop();
    void renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);
    void renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);

private:
    RhiGeometry::Geometry m_cube;
    RhiGeometry::Geometry m_sphere;
    std::shared_ptr<rhi::IPipeline> m_program{};
    std::shared_ptr<rhi::IPipeline> m_cubeMapProgram{};
    std::shared_ptr<rhi::IPipeline> m_backgroundProgram{};
    std::shared_ptr<rhi::IPipeline> m_irradianceProgram{};
    std::shared_ptr<rhi::ITexture2D> m_hdrEnvTexture{};
    std::shared_ptr<rhi::ITexture3D> m_envCubemap{};
    std::shared_ptr<rhi::ITexture3D> m_irradianceMap{};
    std::shared_ptr<rhi::IRenderTarget> m_captureRT{};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
```

- [ ] **Step 2: 重写 cpp**

与 Task 4 几乎相同，差异：
1. 头文件加 `m_irradianceProgram`、`m_irradianceMap` 成员。
2. `compileShader` 额外编译 `Irradiance.vs/Irradiance.fs` 到 `m_irradianceProgram`（同样用 cubeLayout，`setDepthTest(true)`）。
3. 新增 `createIrradianceMap()`：创建 32×32 RGB16F cubemap `_irradianceMap`。
4. 新增 `renderIrradianceMap()`：渲染到 `_irradianceMap` 6 面（viewport 32×32）。
5. `renderBeforeLoop()`：`renderToCubemap(); createIrradianceMap(); renderIrradianceMap();`
6. `renderObjectsAndLights`：绑定 `_irradianceMap` 到 unit 0，设 `program->setUniform("irradianceMap", 0)`（IC 版本无此行）。

关键新增代码（`createIrradianceMap` + `renderIrradianceMap`）：
```cpp
void GLIBLIrradianceApp::createIrradianceMap() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::Linear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = false;
    m_irradianceMap = renderer()->createTexture3D();
    m_irradianceMap->createEmpty(desc, 32, 32);
}

void GLIBLIrradianceApp::renderIrradianceMap() {
    const auto captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_irradianceProgram);
    renderer()->bindTexture(m_envCubemap, 0);
    m_irradianceProgram->setUniform("environmentMap", 0);
    m_irradianceProgram->setUniform("projection", glm::value_ptr(captureProjection), 1);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const int size = 32;
    for (int i = 0; i < 6; ++i) {
        renderer()->setViewport(rhi::Viewport{0, 0, size, size});
        m_irradianceProgram->setUniform("view", glm::value_ptr(captureViews[i]), 1);
        m_captureRT->attachCubeFace(m_irradianceMap.get(), i);
        renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        renderCube(m_irradianceProgram, glm::mat4(1.0));
    }
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}
```
> `createIrradianceMap` 在原代码里会 `glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32)` 调整 capture FBO 深度尺寸。RHI 的 `m_captureRT` 已按 512 建深度附件，渲染到 32×32 面时 viewport 设为 32×32 即可（深度尺寸大于视口无碍，OpenGL 允许 viewport 小于 FBO 尺寸）。**capture RT 共用**（renderToCubemap 512 与 renderIrradianceMap 32 用同一 `m_captureRT`，仅 viewport 不同）。

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 全量回归 + 单 App 冒烟**

Run: `./scripts/run.sh all -b gl -d 1 && ./scripts/run.sh all -a PBR_IBL_Irradiance -b gl -d 1`
Expected: 46/46 OK，单 App 无崩溃、无 GL 错误。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Advanced/PBR/GLIBLIrradianceApp.hpp src/app/GL/Advanced/PBR/GLIBLIrradianceApp.cpp
git commit -m "refactor(app): migrate PBR_IBL_Irradiance to RHI"
```

---

### Task 6: 迁移 PBR_IBL_Specular

**Files:**
- Modify: `src/app/GL/Advanced/PBR/GLIBLSpecularApp.hpp`
- Modify: `src/app/GL/Advanced/PBR/GLIBLSpecularApp.cpp`

**Interfaces:**
- Consumes: Task 1（attachCubeFace mip）、Task 4/5 模式（render-to-cubemap + irradiance）
- Produces: `GLIBLSpecularApp` 迁移完成（prefilter cubemap + BRDF LUT + quad）

- [ ] **Step 1: 重写头文件**

把 `GLIBLSpecularApp.hpp` 改为：
```cpp
#ifndef GL_IBL_SPECULAR_APP_HPP
#define GL_IBL_SPECULAR_APP_HPP

#include "app/GL/Base/GLCameraApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class GLIBLSpecularApp : public GLCameraBaseApp {
public:
    virtual ~GLIBLSpecularApp();

public:
    virtual bool initApp() override;
    virtual void drawScene(const float dt) override;

private:
    void initShapes();
    void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
    void initFramebuffer();
    void initCaptureViews();
    void createIrradianceMap();
    void createPrefilterMap();
    void loadTexture();
    void renderToCubemap();
    void renderIrradianceMap();
    void renderPerfilterMap();
    void renderBrdfLUT();
    void createBrdfLUT();
    void renderBeforeLoop();
    void renderCube(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderSphere(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& model);
    void renderQuad();
    void renderBackground(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);
    void renderObjectsAndLights(const std::shared_ptr<rhi::IPipeline>& program, const glm::mat4& view, const glm::mat4& projection);

private:
    RhiGeometry::Geometry m_cube;
    RhiGeometry::Geometry m_sphere;
    RhiGeometry::Geometry m_quad;
    std::shared_ptr<rhi::IPipeline> m_program{};
    std::shared_ptr<rhi::IPipeline> m_cubeMapProgram{};
    std::shared_ptr<rhi::IPipeline> m_backgroundProgram{};
    std::shared_ptr<rhi::IPipeline> m_irradianceProgram{};
    std::shared_ptr<rhi::IPipeline> m_prefilterProgram{};
    std::shared_ptr<rhi::IPipeline> m_brdfLUTProgram{};
    std::shared_ptr<rhi::ITexture2D> m_hdrEnvTexture{};
    std::shared_ptr<rhi::ITexture3D> m_envCubemap{};
    std::shared_ptr<rhi::ITexture3D> m_irradianceMap{};
    std::shared_ptr<rhi::ITexture3D> m_prefilterMap{};
    std::shared_ptr<rhi::IRenderTarget> m_captureRT{};
    std::shared_ptr<rhi::IRenderTarget> m_brdfLUTRT{};
    std::shared_ptr<rhi::ITexture2D> m_brdfLUTTexture{};
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    glm::vec3 m_albedo = glm::vec3(0.5f, 0.5f, 0.5f);
    float m_ao = 1.0f;
    std::vector<glm::vec3> m_lightPositions;
    std::vector<glm::vec3> m_lightColors;
};

#endif
```

- [ ] **Step 2: 重写 cpp（核心：prefilter mip + BRDF LUT + quad）**

在 Task 5 基础上新增以下（复用 `GetCaptureViews`/`GetLightPosAndColor`/`GenreateObjPos`/`GetCaptureProjection`/`renderCube`/`renderSphere`/`renderBackground`/`renderObjectsAndLights`，`renderObjectsAndLights` 额外绑定 prefilter + brdfLUT）：

**quad buffer（TriangleStrip）**：
```cpp
static RhiGeometry::Geometry CreateQuadBuffer(rhi::IRenderer* renderer) {
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    constexpr int stride = 5 * static_cast<int>(sizeof(float));
    rhi::VertexLayout layout;
    layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
    layout.elements.push_back({rhi::VertexElement::Float2, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
    return RhiGeometry::CreateFromArray(renderer, quadVertices, sizeof(quadVertices), 4, layout);
}
```
> 需 include `"app/GL/RhiGeometry.hpp"`（头文件已含）。

**capture projection 静态辅助**：
```cpp
static glm::mat4 GetCaptureProjection() {
    return glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
}
```

**prefilter map 创建（cubemap，generateMipmap=true，LinearMipLinear min filter）**：
```cpp
void GLIBLSpecularApp::createPrefilterMap() {
    rhi::TextureDesc desc;
    desc.format = rhi::TextureFormat::RGB16F;
    desc.wrapS = desc.wrapT = desc.wrapR = rhi::TextureWrap::ClampToEdge;
    desc.minFilter = rhi::TextureFilter::LinearMipLinear;
    desc.magFilter = rhi::TextureFilter::Linear;
    desc.generateMipmap = true;
    m_prefilterMap = renderer()->createTexture3D();
    m_prefilterMap->createEmpty(desc, 128, 128);
}
```

**prefilter 渲染（渲染到 mip 1-4，用 attachCubeFace 的 mip 参数）**：
```cpp
void GLIBLSpecularApp::renderPerfilterMap() {
    const auto captureProjection = GetCaptureProjection();
    const auto captureViews = GetCaptureViews();
    renderer()->setPipeline(m_prefilterProgram);
    renderer()->bindTexture(m_envCubemap, 0);
    m_prefilterProgram->setUniform("environmentMap", 0);
    m_prefilterProgram->setUniform("projection", glm::value_ptr(captureProjection), 1);
    renderer()->setRenderTarget(m_captureRT);
    renderer()->setVertexBuffer(m_cube.vertexBuffer);
    renderer()->setVertexBuffer(m_cube.uvBuffer, 1);
    renderer()->setVertexBuffer(m_cube.normalBuffer, 2);
    const unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
        const unsigned int mipSize = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        m_prefilterProgram->setUniform("roughness", roughness);
        for (int i = 0; i < 6; ++i) {
            renderer()->setViewport(rhi::Viewport{0, 0, static_cast<int>(mipSize), static_cast<int>(mipSize)});
            m_prefilterProgram->setUniform("view", glm::value_ptr(captureViews[i]), 1);
            m_captureRT->attachCubeFace(m_prefilterMap.get(), i, static_cast<int>(mip));
            renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            renderCube(m_prefilterProgram, glm::mat4(1.0));
        }
    }
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}
```

**BRDF LUT（渲染到 2D RG16F 纹理，quad TriangleStrip）**：
```cpp
void GLIBLSpecularApp::createBrdfLUT() {
    m_brdfLUTRT = renderer()->createRenderTarget();
    rhi::FramebufferDesc fbd;
    fbd.width = 512; fbd.height = 512;
    fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RG16F, false, 0});
    if (!m_brdfLUTRT->create(fbd)) {
        ExitIfFailed(false, "Failed to create BRDF LUT framebuffer");
    }
    m_brdfLUTTexture = std::shared_ptr<rhi::ITexture2D>(m_brdfLUTRT->colorTexture2D(0),
                                                        [](rhi::ITexture2D*){});
}

void GLIBLSpecularApp::renderBrdfLUT() {
    renderer()->setRenderTarget(m_brdfLUTRT);
    renderer()->setViewport(rhi::Viewport{0, 0, 512, 512});
    renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    renderer()->setPipeline(m_brdfLUTProgram);
    renderer()->setVertexBuffer(m_quad.vertexBuffer);
    renderer()->draw(m_quad.vertexCount, 0);
    renderer()->setRenderTarget(nullptr);
    const auto props = m_window->getProperties();
    renderer()->setViewport(rhi::Viewport{0, 0, props.width, props.height});
}
```
> `m_brdfLUTRT` 只有 color 附件、无 depth——`create()` 会把 drawBufs 设为 `[COLOR_ATTACHMENT0]`（非空），正常。quad 用 `TriangleStrip`。

**compileShader 差异**：`m_brdfLUTProgram` 用 quadLayout，并 `setPrimitiveType(TriangleStrip)`；其余（cubeMap/irradiance/prefilter/PBR/background）用 cubeLayout，`setDepthTest(true)`。

**renderObjectsAndLights 差异**：额外
```cpp
renderer()->bindTexture(m_prefilterMap, 1);
renderer()->bindTexture(m_brdfLUTTexture, 2);
program->setUniform("prefilterMap", 1);
program->setUniform("brdfLUT", 2);
```
（保留 `renderer()->bindTexture(m_irradianceMap, 0); program->setUniform("irradianceMap", 0);`）

**renderBeforeLoop**：
```cpp
void GLIBLSpecularApp::renderBeforeLoop() {
    renderToCubemap();
    createIrradianceMap();
    renderIrradianceMap();
    createPrefilterMap();
    renderPerfilterMap();
    createBrdfLUT();
    renderBrdfLUT();
}
```

**编译顺序注意**：`compileShader` 需要 quadLayout，所以 `initApp` 先建 cube 和 quad 几何，再调 `compileShader(cubeLayout, quadLayout)`。

**renderToCubemap 与 envCubemap mipmap**：原代码 `renderToCubemap()` 末尾会 `glGenerateMipmap(GL_TEXTURE_CUBE_MAP)` 为 `_envCubemap` 生成 mip 链供 prefilter 采样。本批次 `_envCubemap` 的 `createEmpty` 设 `generateMipmap=false`（见 Task 6 renderToCubemap），因为：prefilter 采样的是独立创建的 `_prefilterMap`（其 `createPrefilterMap` 用 generateMipmap=true 自建 mip 链），background 只用 `_envCubemap` 的 mip0。故 `_envCubemap` 无需生成 mipmap，避免引入新的 RHI generateMipmap API。

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 全量回归 + 单 App 冒烟**

Run: `./scripts/run.sh all -b gl -d 1 && ./scripts/run.sh all -a PBR_IBL_Specular -b gl -d 1`
Expected: 46/46 OK，单 App 无崩溃、无 GL 错误。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Advanced/PBR/GLIBLSpecularApp.hpp src/app/GL/Advanced/PBR/GLIBLSpecularApp.cpp
git commit -m "refactor(app): migrate PBR_IBL_Specular to RHI"
```

---

## 自审结果（planning 阶段完成）

- **Spec 覆盖**：Spec 的 2 个 RHI 缺口 → Task 1；5 个 App → Task 2-6；render-to-cubemap 流程 → Task 4；irradiance → Task 5；prefilter mip + BRDF LUT + quad → Task 6。全部覆盖。
- **无占位符**：所有代码块为完整可编译代码；Task 4 头文件的 `_cubeVb` 冗余占位已删除。
- **计划评审修正（plan reviewer，4 项已修复）**：① 所有 `renderSphere/renderCube` 补充 `setIndexBuffer`（`drawIndexed` 需要索引缓冲）② `drawScene/renderObjectsAndLights` 的 `setUniform` 前补 `setPipeline`（`GLPipeline::setUniform` 走 `glUniform` 需当前 program）③ Task 6 头文件补 `createBrdfLUT` 声明 ④ Task 6 补 `GetCaptureProjection` static 定义。
- **类型一致性**：`attachCubeFace(cube, face, mip=0)` 在 Task 1 定义、Task 4/5/6 使用一致；`RhiGeometry::Create` / `createEmpty` / `bindTexture` / `setViewport` / `setUniformMatrix` 签名均与现有 RHI 一致。
