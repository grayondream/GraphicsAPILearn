# RHI Migration Phase D — FBO / Post-processing (Hdr/Bloom/Defer) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 3 个 Light/Advanced 后处理 App（Hdr、Bloom、Defer）从原生 GLProgram 迁移到 RHI，并扩展 RHI 支持帧缓冲后处理所需的深度-only blit、附件采样参数、raw 纹理绑定。

**Architecture:** 复用计划B/C 确立的 `RhiGeometry::Create`/`CreateFromArray` + 管线模板。全屏 quad（pos/uv 交错）用 `CreateFromArray`；Cube/Plane 用 `RhiGeometry::Create`。本批次首次引入 `IRenderTarget`（多附件 FBO）用于 render-to-texture：T1 扩展 RHI（BlitMask 枚举掩码、FramebufferAttachment filter/wrap、bindTexture raw 重载），T2-T4 分别迁移 Hdr/Bloom/Defer。

**Tech Stack:** C++17、RHI 抽象层（IRenderer/IPipeline/IShader/ITexture2D/IRenderTarget/IBuffer）、glm、imgui。

## Global Constraints

- **红线：** 每任务 `./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1` 必须 46/46 OK。
- **不重命名** 类名/AppType；只改本任务涉及文件。
- 禁用 GLProgram/glad/GLImageTexture2D/GLCube/GLPlane include；用 `renderer()` RHI 调用。
- `PROGRESS.md` 更新到本地（gitignored，不入库）；代码提交链 `docs` 先行。
- shader 路径：`join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "<Dir>")`；shader 为 `.vs/.fs` 扩展名。
- vec4 用 `setUniform(name, glm::value_ptr(v), 1, 4)`；vec3 用 `setUniform(name, glm::value_ptr(v), 1, 3)`；bool 用 bool 重载；int 用 int 重载；矩阵用 `setUniform(name, glm::value_ptr(m), 1)`。
- 文件纹理 `RhiImage::Load2D(renderer().get(), file)`；绑定 `renderer()->bindTexture(tex, unit)`。
- 窗口尺寸用 `m_window->getProperties().width / .height`（GLCameraBaseApp 的 Application 基类成员）。
- FBO 后处理：`renderer()->createRenderTarget()` 建 RT，`RT->create(FramebufferDesc)` 建 FBO，`renderer()->setRenderTarget(RT)` 渲染进 FBO，`renderer()->setRenderTarget(nullptr)` 回默认 framebuffer。
- 全屏 quad 布局：pos(Float3, semantic0, off0) + uv(Float2, semantic1, off12)，stride 20，`TriangleStrip` 4 顶点，`renderer()->draw(4, 0)`。
- 渲染到 FBO 的几何（Hdr cube / Bloom cube+plane / Defer cube+plane）需 `setVertexBuffer` 对应 geometry 各 buffer 并按 layout binding 挂接。

---

### Task 1: RHI 扩展 — BlitMask、Attachment filter/wrap、bindTexture raw 重载

**Files:**
- Modify: `src/rhi/core/Common.hpp`（新增 BlitMask 枚举、FramebufferAttachment 字段）
- Modify: `src/rhi/core/IRenderer.hpp`（blitFramebuffer 签名、bindTexture raw 重载）
- Modify: `src/rhi/gl/GLBackend.cpp`（blitFramebuffer 掩码实现、bindTexture raw 实现）
- Modify: `src/rhi/gl/GLRenderTarget.cpp`（create(desc) 用 attachment filter/wrap）
- Test: 无独立测试文件，用现有 App 编译 + 回归验证

**Interfaces:**
- Consumes: 现有 `FramebufferDesc`/`FramebufferAttachment`/`TextureFilter`/`TextureWrap`/`TextureFormat`（Common.hpp）。
- Produces:
  - `enum class BlitMask : uint8_t { None=0, Color=1, Depth=2 }`（Common.hpp）。
  - `IRenderer::blitFramebuffer(src, dst, mask = BlitMask::Color | BlitMask::Depth)`。
  - `IRenderer::bindTexture(rhi::ITexture2D* texture, unsigned int unit = 0)`（新重载）。
  - `FramebufferAttachment` 新增 `minFilter/magFilter/wrapS/wrapT` 字段（默认 Linear / ClampToEdge）。

- [ ] **Step 1: 在 `Common.hpp` 添加 `BlitMask` 枚举**

在 `TextureFormat` 枚举（约 line 46）后新增：

```cpp
// blitFramebuffer 拷贝哪些缓冲位
enum class BlitMask : uint8_t { None = 0, Color = 1, Depth = 2 };
```

- [ ] **Step 2: 给 `FramebufferAttachment` 增加采样参数字段**

`FramebufferAttachment` 结构（约 line 65）改为：

```cpp
struct FramebufferAttachment {
    AttachmentType type{AttachmentType::Color};
    TextureFormat format{TextureFormat::RGBA8};
    bool external{false};        // true=由 App 提供纹理句柄，false=内部创建
    int samples{0};              // >0 时 MSAA
    TextureFilter minFilter{TextureFilter::Linear};
    TextureFilter magFilter{TextureFilter::Linear};
    TextureWrap wrapS{TextureWrap::ClampToEdge};
    TextureWrap wrapT{TextureWrap::ClampToEdge};
};
```

- [ ] **Step 3: 更新 `IRenderer.hpp` 的 blitFramebuffer 签名与 bindTexture 重载**

`blitFramebuffer`（line 56）改为带默认 mask；`bindTexture`（line 48 后）增加 raw 指针重载：

```cpp
    virtual void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                                 const std::shared_ptr<IRenderTarget>& dst,
                                 BlitMask mask = static_cast<BlitMask>(static_cast<uint8_t>(BlitMask::Color) |
                                                                       static_cast<uint8_t>(BlitMask::Depth))) = 0;
    virtual void bindTexture(const std::shared_ptr<ITexture2D>& texture, unsigned int unit = 0) = 0;
    virtual void bindTexture(rhi::ITexture2D* texture, unsigned int unit = 0) = 0;     // 新增：raw 指针（RT 颜色纹理）
```

- [ ] **Step 4: 实现 `GLBackend.cpp` 的 blitFramebuffer 掩码 + bindTexture raw 重载**

`blitFramebuffer`（line 111-121）改为按 mask 拼接位：

```cpp
    void blitFramebuffer(const std::shared_ptr<IRenderTarget>& src,
                         const std::shared_ptr<IRenderTarget>& dst,
                         BlitMask mask = static_cast<BlitMask>(static_cast<uint8_t>(BlitMask::Color) |
                                                               static_cast<uint8_t>(BlitMask::Depth))) override {
        if (!src) return;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,
                          static_cast<GLuint>(reinterpret_cast<uintptr_t>(src->handle())));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                          dst ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dst->handle())) : 0);
        GLbitfield bits = 0;
        if (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Color)) bits |= GL_COLOR_BUFFER_BIT;
        if (static_cast<uint8_t>(mask) & static_cast<uint8_t>(BlitMask::Depth)) bits |= GL_DEPTH_BUFFER_BIT;
        glBlitFramebuffer(0, 0, _viewportW, _viewportH, 0, 0, _viewportW, _viewportH, bits, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
```

在 `bindTexture`（shared_ptr 版，line 84-89）后新增 raw 指针重载：

```cpp
    void bindTexture(rhi::ITexture2D* texture, unsigned int unit) override {
        if (texture) texture->bind(unit);
    }
```

- [ ] **Step 5: 实现 `GLRenderTarget::create(desc)` 用 attachment filter/wrap**

`GLRenderTarget.cpp` 的 `create(const FramebufferDesc&)` 中，颜色纹理采样参数（line 69-70）由硬编码改为用 attachment 字段：

```cpp
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLFilter(a.minFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ToGLFilter(a.magFilter));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(a.wrapS));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(a.wrapT));
```

深度附件分支（line 95-98）保持 Nearest + ClampToEdge（与现行为一致）不变。

- [ ] **Step 6: 添加 `ToGLFilter` / `ToGLWrap` 辅助函数**

`GLRenderTarget.cpp` 文件顶部（`create` 之前）新增：

```cpp
namespace {
GLint ToGLFilter(rhi::TextureFilter f) {
    switch (f) {
        case rhi::TextureFilter::Nearest: return GL_NEAREST;
        case rhi::TextureFilter::LinearMipLinear: return GL_LINEAR_MIPMAP_LINEAR;
        default: return GL_LINEAR;
    }
}
GLint ToGLWrap(rhi::TextureWrap w) {
    switch (w) {
        case rhi::TextureWrap::Repeat: return GL_REPEAT;
        case rhi::TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
        default: return GL_CLAMP_TO_EDGE;
    }
}
} // namespace
```

（若 `ToGLFilter`/`ToGLWrap` 已存在文件级或公共 util，复用；否则在此定义。）

- [ ] **Step 7: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过，无新增 error/warning。

- [ ] **Step 8: 回归确认现有行为未破坏**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

- [ ] **Step 9: 提交**

```bash
git add src/rhi/core/Common.hpp src/rhi/core/IRenderer.hpp src/rhi/gl/GLBackend.cpp src/rhi/gl/GLRenderTarget.cpp
git commit -m "feat(rhi): add BlitMask, attachment filter/wrap, bindTexture raw overload for postprocess FBO"
```

---

### Task 2: 迁移 Hdr

**Files:**
- Modify: `src/app/GL/Light/Advanced/GLHdrApp.hpp`（成员改 RHI 类型）
- Rewrite: `src/app/GL/Light/Advanced/GLHdrApp.cpp`（GL→RHI）
- Test: build + 46/46 + `./scripts/run.sh all -a Hdr -b gl -d 1`

**Interfaces:**
- Consumes: T1 的 `BlitMask`、`bindTexture(raw)`、`FramebufferAttachment` filter/wrap；`RhiGeometry::CreateFromArray`、`RhiImage::Load2D`。
- Produces: Hdr 的 FBO 颜色纹理经 `RT->colorTexture2D(0)` 采样给后处理 quad。

**Shader 布局：** `Lighting.vs` 用 `pos=0, normal=1, uv=2`（cube）；`Hdr.vs` 用 `pos=0, uv=1`（quad）。

- [ ] **Step 1: 改写 `GLHdrApp.hpp` 成员为 RHI 类型**

```cpp
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"

class GLHdrApp : public GLCameraBaseApp {
public:
	virtual ~GLHdrApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void render2FrameBuffer();
	void renderHdr();

private:
	std::shared_ptr<rhi::IPipeline> _objPipeline{};
	std::shared_ptr<rhi::IPipeline> _hdrPipeline{};
	std::shared_ptr<rhi::ITexture2D> _brick{};

	std::shared_ptr<rhi::IBuffer> _vb{};            // cube 顶点
	uint32_t _cubeVertexCount{0};
	std::shared_ptr<rhi::IBuffer> _quadVb{};        // 全屏 quad 顶点
	uint32_t _quadVertexCount{0};

	std::shared_ptr<rhi::IRenderTarget> _hdrRT{};
	std::shared_ptr<rhi::ITexture2D> _colorBuffer{};   // = _hdrRT->colorTexture2D(0)

	bool _enableHdr = true;
	float _exposure = 0.5f;
};
```

- [ ] **Step 2: 改写 `GLHdrApp.cpp`**

`#include` 改为 RHI 头；删除 GLProgram/glad/GLImageTexture2D/GLUtils include。`CreateRectBuffer`（36 顶点 cube，8 float/顶点）改为返回 `RhiGeometry::Geometry`，用 `CreateFromArray` + 自定义布局（pos@0 Float3 off0, normal@1 Float3 off12, uv@2 Float2 off24, stride 32）。`CreateScreenVao`（quad，5 float/顶点）改为 `CreateFromArray` + 布局（pos@0 Float3 off0, uv@1 Float2 off12, stride 20）。`CreateRenderFrame` 改为 `IRenderTarget::create(FramebufferDesc)`。

```cpp
#include "GLHdrApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/Common.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLHdrApp::~GLHdrApp() {
}

static RhiGeometry::Geometry CreateCubeBuffer(rhi::IRenderer* renderer) {
	// 原始 36 顶点 cube：每顶点 8 float（pos3 + normal3 + uv2），与 GLHdrApp.cpp 原 CreateRectBuffer 相同
	float vertices[] = {
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
		1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		1.0f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
	};
	constexpr int stride = 8 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 24, stride});
	return RhiGeometry::CreateFromArray(renderer, vertices, sizeof(vertices), 36, layout);
}

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

bool GLHdrApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	auto cube = CreateCubeBuffer(renderer().get());
	_vb = cube.vertexBuffer;
	_cubeVertexCount = cube.vertexCount;
	const auto cubeLayout = cube.layout;

	auto quad = CreateQuadBuffer(renderer().get());
	_quadVb = quad.vertexBuffer;
	_quadVertexCount = quad.vertexCount;
	const auto quadLayout = quad.layout;

	compileShader(cubeLayout, quadLayout);

	const int w = static_cast<int>(m_window->getProperties().width);
	const int h = static_cast<int>(m_window->getProperties().height);
	_hdrRT = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!_hdrRT->create(fbd)) {
		ExitIfFailed(false, "Failed to create Hdr framebuffer");
	}
	_colorBuffer = std::shared_ptr<rhi::ITexture2D>(_hdrRT->colorTexture2D(0), [](rhi::ITexture2D*){});

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_brick = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_brick != nullptr, "Failed to load texture from file {}", imgFile);
	}
	return true;
}

void GLHdrApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "Hdr");
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Lighting.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Lighting.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_objPipeline = renderer()->createPipeline(cubeLayout, shader);
		_objPipeline->setDepthTest(true);
	}
	{
		auto shader = renderer()->createShader();
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, join(shaderDir, "Hdr.vs"), "main", false},
		                            {rhi::ShaderStage::Fragment, join(shaderDir, "Hdr.fs"), "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_hdrPipeline = renderer()->createPipeline(quadLayout, shader);
	}
}

`render2FrameBuffer` / `renderHdr` 迁移：

```cpp
void GLHdrApp::render2FrameBuffer() {
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	renderer()->setRenderTarget(_hdrRT);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(_objPipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->bindTexture(_brick, 0);
	_objPipeline->setUniform("diffuseTexture", 0);
	_objPipeline->setUniform("projection", glm::value_ptr(projection), 1);
	_objPipeline->setUniform("view", glm::value_ptr(view), 1);
	auto [lightPositions, lightColors] = GetLightPosColor();
	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		_objPipeline->setUniform("lights[" + std::to_string(i) + "].Position",
		                         glm::value_ptr(lightPositions[i]), 1, 3);
		_objPipeline->setUniform("lights[" + std::to_string(i) + "].Color",
		                         glm::value_ptr(lightColors[i]), 1, 3);
	}
	_objPipeline->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 25.0));
	model = glm::scale(model, glm::vec3(2.5f, 2.5f, 27.5f));
	_objPipeline->setUniform("model", glm::value_ptr(model), 1);
	_objPipeline->setUniform("inverse_normals", true);
	renderer()->draw(_cubeVertexCount, 0);
	renderer()->setRenderTarget(nullptr);
}

void GLHdrApp::renderHdr() {
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(_hdrPipeline);
	renderer()->bindTexture(_hdrRT->colorTexture2D(0), 0);
	_hdrPipeline->setUniform("hdrBuffer", 0);
	_hdrPipeline->setUniform("hdr", _enableHdr);
	_hdrPipeline->setUniform("exposure", _exposure);
	renderer()->setVertexBuffer(_quadVb);
	renderer()->draw(_quadVertexCount, 0);
}

void GLHdrApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Hdr", &_enableHdr);
	ImGui::InputFloat("Exposure", &_exposure, 0.1f, 4.0f, "%.2f");
	ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", _camera.getAttr().pos.x, _camera.getAttr().pos.y, _camera.getAttr().pos.z);
	ImGui::End();

	render2FrameBuffer();
	renderHdr();
}
```

> 保留原 `GetLightPosColor()` static 函数。注意 `renderHdr` 里 `bindTexture(raw)` 需用 T1 的 raw 重载（`_hdrRT->colorTexture2D(0)` 返回 `rhi::ITexture2D*`）。若需避免重复绑定可改用持有的 `_colorBuffer`（shared_ptr 版），两者皆可。

- [ ] **Step 3: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。若 `bindTexture` raw 重载未生效（冲突），确认 T1 已合并；`colorTexture2D` 需 `#include "rhi/core/IRenderTarget.hpp"`。

- [ ] **Step 4: 回归 + 单跑 Hdr**

Run: `./scripts/run.sh all -b gl -d 1`，Expected 46/46 OK。
Run: `./scripts/run.sh all -a Hdr -b gl -d 1`，Expected [ OK ] Hdr。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Light/Advanced/GLHdrApp.hpp src/app/GL/Light/Advanced/GLHdrApp.cpp
git commit -m "refactor(app): migrate Hdr to RHI (FBO render-to-texture + HDR postprocess)"
```

---

### Task 3: 迁移 Bloom

**Files:**
- Modify: `src/app/GL/Light/Advanced/GLBloomApp.hpp`（成员改 RHI 类型）
- Rewrite: `src/app/GL/Light/Advanced/GLBloomApp.cpp`（GL→RHI）
- Test: build + 46/46 + `./scripts/run.sh all -a Bloom -b gl -d 1`

**Interfaces:**
- Consumes: T1 的 `BlitMask`（本任务不用深度 blit）、`bindTexture(raw)`、`FramebufferAttachment` filter/wrap；T2 的 quad buffer 模式；`RhiGeometry::Create`（Cube/Plane shape）、`RhiImage::Load2D`。
- Produces: HDR FBO（2 颜色附件）+ 2 个 pingpong FBO；Bloom 用 `colorTexture2D(0)/(1)` 采样。

**Shader 布局：** `Bloom.vs`/`Light.vs` 用 `pos=0,color=1,uv=2,normal=3`（shape，与 RhiGeometry::Create 默认一致）；`Blur.vs`/`Final.vs` 用 `pos=0,uv=1`（quad）。

- [ ] **Step 1: 改写 `GLBloomApp.hpp` 成员为 RHI 类型**

```cpp
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Plane.hpp"

class GLBloomApp : public GLCameraBaseApp {
public:
	virtual ~GLBloomApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void initShapes();
	void createTextures();
	void createQuadBuffer();

	void extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view);
	void blurBrightPart();
	void renderFinal();
	void renderQuad();

private:
	std::shared_ptr<rhi::IPipeline> m_bloomProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightProgram{};
	std::shared_ptr<rhi::IPipeline> m_blurProgram{};
	std::shared_ptr<rhi::IPipeline> m_finalProgram{};

	std::shared_ptr<rhi::IRenderTarget> m_hdrFBO{};
	std::array<std::shared_ptr<rhi::IRenderTarget>, 2> m_pingpongFBO{};

	std::shared_ptr<rhi::ITexture2D> m_woodTexture{};
	std::shared_ptr<rhi::ITexture2D> m_brickTexture{};

	Cube m_cube{};
	Plane m_plane{};
	std::shared_ptr<rhi::IBuffer> m_cubeVb{}, m_cubeUv{}, m_cubeNormal{}, m_cubeEbo{};
	uint32_t m_cubeIndexCount{0};
	std::shared_ptr<rhi::IBuffer> m_planeVb{}, m_planeUv{}, m_planeNormal{};
	uint32_t m_planeVertexCount{0};
	std::shared_ptr<rhi::IBuffer> m_quadVb{};
	uint32_t m_quadVertexCount{0};

	bool m_enableBloom{};
	float m_expose{};
};
```

- [ ] **Step 2: 改写 `GLBloomApp.cpp`**

`#include` 改为 RHI 头，删除 GLProgram/glad/GLImageTexture2D/GLCube/GLPlane/GLUtils include。`initShapes` 改为直接用 `RhiGeometry::Create`：

```cpp
void GLBloomApp::initShapes() {
	// Cube：pos+color+uv+normal indexed，layout 默认（uv=2/normal=3）
	auto cubeGeo = RhiGeometry::Create(renderer().get(), m_cube, true, true, true);
	m_cubeVb = cubeGeo.vertexBuffer;
	m_cubeUv = cubeGeo.uvBuffer;
	m_cubeNormal = cubeGeo.normalBuffer;
	m_cubeEbo = cubeGeo.indexBuffer;
	m_cubeIndexCount = cubeGeo.indexCount;

	// Plane：pos+color+uv+normal，drawArrays 6
	auto planeGeo = RhiGeometry::Create(renderer().get(), m_plane, true, true, false);
	m_planeVb = planeGeo.vertexBuffer;
	m_planeUv = planeGeo.uvBuffer;
	m_planeNormal = planeGeo.normalBuffer;
	m_planeVertexCount = planeGeo.vertexCount;
}
```

`compileShader` 里 4 个管线分别用对应布局：light/bloom 用 cube 布局（`RhiGeometry::Create` 默认 layout），blur/final 用 quad 布局。签名：

```cpp
void GLBloomApp::compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout) {
	// m_lightProgram / m_bloomProgram 用 cubeLayout；m_blurProgram / m_finalProgram 用 quadLayout
}
```

`initApp` 中先 `initShapes()`（得到 cubeLayout）再 `createQuadBuffer()`（得到 quadLayout）再 `compileShader(cubeLayout, quadLayout)`，顺序不可颠倒。

FBO 创建：

```cpp
// HDR FBO：2 个 RGBA16F 颜色附件（ClampToEdge，默认已 ClampToEdge）+ 深度
_hdrFBO = renderer()->createRenderTarget();
rhi::FramebufferDesc fbd;
fbd.width = w; fbd.height = h;
fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});  // colorBuffers[0]
fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});  // colorBuffers[1]
fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
if (!_hdrFBO->create(fbd)) ExitIfFailed(false, "Failed to create Hdr framebuffer");

// pingpong FBO：2 个，各 1 个 RGBA16F 颜色附件（ClampToEdge，无深度）
for (int i = 0; i < 2; i++) {
	m_pingpongFBO[i] = renderer()->createRenderTarget();
	rhi::FramebufferDesc pbd;
	pbd.width = w; pbd.height = h;
	pbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0});
	if (!m_pingpongFBO[i]->create(pbd)) ExitIfFailed(false, "Failed to create pingpong framebuffer");
}
```

`createQuadBuffer` 用 `CreateQuadBuffer`（与 Hdr 相同）。绘制迁移，关键流程：

```cpp
void GLBloomApp::extractBrightPart(const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setRenderTarget(m_hdrFBO);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderLight(m_lightProgram, projection, view);
	const auto viewPos = _camera.getAttr().pos;
	const auto [lightPositions, lightColors] = GetLightPosAndColor();
	for (int i = 0; i < lightPositions.size(); i++) {
		m_bloomProgram->setUniform("lights[" + std::to_string(i) + "].Position",
		                           glm::value_ptr(lightPositions[i]), 1, 3);
		m_bloomProgram->setUniform("lights[" + std::to_string(i) + "].Color",
		                           glm::value_ptr(lightColors[i]), 1, 3);
	}
	renderCubes(m_bloomProgram, projection, view, viewPos);
	renderPlane(m_bloomProgram, projection, view, viewPos);
	renderer()->setRenderTarget(nullptr);
}

void GLBloomApp::blurBrightPart() {
	bool horizontal = true, first_iteration = true;
	for (unsigned int i = 0; i < 10; i++) {
		renderer()->setRenderTarget(m_pingpongFBO[horizontal]);
		renderer()->setPipeline(m_blurProgram);
		renderer()->bindTexture(first_iteration ? m_hdrFBO->colorTexture2D(1)
		                                        : m_pingpongFBO[!horizontal]->colorTexture2D(0), 0);
		m_blurProgram->setUniform("image", 0);
		m_blurProgram->setUniform("horizontal", horizontal);
		renderQuad();
		horizontal = !horizontal;
		first_iteration = false;
	}
	renderer()->setRenderTarget(nullptr);
}

void GLBloomApp::renderFinal() {
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(m_finalProgram);
	renderer()->bindTexture(m_hdrFBO->colorTexture2D(0), 0);
	m_finalProgram->setUniform("scene", 0);
	renderer()->bindTexture(m_pingpongFBO[1]->colorTexture2D(0), 1);
	m_finalProgram->setUniform("bloomBlur", 1);
	m_finalProgram->setUniform("bloom", m_enableBloom);
	m_finalProgram->setUniform("exposure", m_expose);
	renderQuad();
}

void GLBloomApp::renderQuad() {
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}
```

> 注意：`renderQuad` **不**切换 pipeline（每个调用方在调用前已 `setPipeline` 自己的管线，与原 GL 代码一致——原 `renderQuad` 只绑 VAO 并 draw）。尤其 `blurBrightPart` 里 `renderQuad` 必须沿用 `m_blurProgram`，若在此改绑 Final 会跑错 shader。

> 注意：`renderOneCube` 绘制 cube 时 `setVertexBuffer(m_cubeVb)` + `setVertexBuffer(m_cubeUv,1)` + `setVertexBuffer(m_cubeNormal,2)` + `setIndexBuffer(m_cubeEbo)` + `drawIndexed(m_cubeIndexCount,0,0)`；`renderPlane` 用 `draw(m_planeVertexCount, 0)`（drawArrays）。`renderLight` 用 `m_lightProgram` 绘制代表光源的小立方体。保持原有 4 个管线各司其职。

- [ ] **Step 3: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 回归 + 单跑 Bloom**

Run: `./scripts/run.sh all -b gl -d 1`，Expected 46/46 OK。
Run: `./scripts/run.sh all -a Bloom -b gl -d 1`，Expected [ OK ] Bloom。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Light/Advanced/GLBloomApp.hpp src/app/GL/Light/Advanced/GLBloomApp.cpp
git commit -m "refactor(app): migrate Bloom to RHI (multi-attachment FBO + pingpong blur)"
```

---

### Task 4: 迁移 Defer

**Files:**
- Modify: `src/app/GL/Light/Advanced/GLDeferApp.hpp`（成员改 RHI 类型）
- Rewrite: `src/app/GL/Light/Advanced/GLDeferApp.cpp`（GL→RHI）
- Test: build + 46/46 + `./scripts/run.sh all -a Defer -b gl -d 1`

**Interfaces:**
- Consumes: T1 的 `BlitMask::Depth`（深度-only blit）、`bindTexture(raw)`、`FramebufferAttachment` filter/wrap；`RhiGeometry::Create`、`RhiImage::Load2D`。
- Produces: GBuffer（3 颜色附件，Nearest）+ 深度 blit 到默认 FBO。

**Shader 布局：** `GBuffer.vs`/`LightBox.vs` 用 `pos=0,color=1,uv=2,normal=3`（shape）；`Light.vs` 用 `pos=0,uv=1`（quad）。

- [ ] **Step 1: 改写 `GLDeferApp.hpp` 成员为 RHI 类型**

```cpp
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IRenderer.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Cube.hpp"
#include "geometry/Plane.hpp"

class GLDeferApp : public GLCameraBaseApp {
public:
	virtual ~GLDeferApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout);
	void initShapes();
	void createTextures();
	void createFrameBuffers();
	void createQuadBuffer();

	void renderLightBox(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);
	void renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);
	void renderOneCube(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &model, const glm::mat4 &projection, const glm::mat4 &view);
	void renderCubes(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderPlane(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &viewPos);
	void renderQuad();
	void renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view);

private:
	std::shared_ptr<rhi::IRenderTarget> m_gBuffer{};

	std::shared_ptr<rhi::IPipeline> m_gBufferProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightBoxProgram{};
	std::shared_ptr<rhi::IPipeline> m_lightProgram{};

	int m_Count{10};

	std::shared_ptr<rhi::ITexture2D> m_woodTexture{};
	std::shared_ptr<rhi::ITexture2D> m_brickTexture{};

	Cube m_cube{};
	Plane m_plane{};
	std::shared_ptr<rhi::IBuffer> m_cubeVb{}, m_cubeUv{}, m_cubeNormal{}, m_cubeEbo{};
	uint32_t m_cubeIndexCount{0};
	std::shared_ptr<rhi::IBuffer> m_planeVb{}, m_planeUv{}, m_planeNormal{};
	uint32_t m_planeVertexCount{0};
	std::shared_ptr<rhi::IBuffer> m_quadVb{};
	uint32_t m_quadVertexCount{0};

	bool m_enableVolume{};
};
```

- [ ] **Step 2: 改写 `GLDeferApp.cpp`**

`#include` 改为 RHI 头，删除 GLProgram/glad/GLImageTexture2D/GLCube/GLPlane/GLUtils include。`initShapes`/`createQuadBuffer` 同 Bloom（Cube/Plane 用 `RhiGeometry::Create`，quad 用 `CreateFromArray`）。`compileShader` 用签名 `compileShader(const rhi::VertexLayout& cubeLayout, const rhi::VertexLayout& quadLayout)`；`initApp` 顺序为 `initShapes()` → `createQuadBuffer()` → `compileShader(cubeLayout, quadLayout)`（先建 buffer 得到 layout）。FBO 创建：

```cpp
void GLDeferApp::createFrameBuffers() {
	const int w = static_cast<int>(m_window->getProperties().width);
	const int h = static_cast<int>(m_window->getProperties().height);
	m_gBuffer = renderer()->createRenderTarget();
	rhi::FramebufferDesc fbd;
	fbd.width = w; fbd.height = h;
	// 3 颜色附件，Nearest filter（避免 GBuffer 线性插值模糊）
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA16F, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Color, rhi::TextureFormat::RGBA8, false, 0,
	                           rhi::TextureFilter::Nearest, rhi::TextureFilter::Nearest,
	                           rhi::TextureWrap::ClampToEdge, rhi::TextureWrap::ClampToEdge});
	fbd.attachments.push_back({rhi::AttachmentType::Depth, rhi::TextureFormat::Depth24Stencil8, false, 0});
	if (!m_gBuffer->create(fbd)) ExitIfFailed(false, "Failed to create GBuffer framebuffer");
}
```

> 说明：`FramebufferAttachment` 聚合初始化顺序为 `{type, format, external, samples, minFilter, magFilter, wrapS, wrapT}`。深度附件（无 filter 需求）用默认即 Nearest/ClampToEdge。

`renderGBuffer` 迁移（几何 pass 写 3 附件）：

```cpp
void GLDeferApp::renderGBuffer(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setRenderTarget(m_gBuffer);
	renderer()->clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	renderer()->setPipeline(program);
	program->setUniform("projection", glm::value_ptr(projection), 1);
	program->setUniform("view", glm::value_ptr(view), 1);
	renderer()->bindTexture(m_woodTexture, 0);
	program->setUniform("diffuseTexture", 0);
	renderCubes(program, projection, view, _camera.getAttr().pos);
	renderer()->bindTexture(m_brickTexture, 0);
	program->setUniform("diffuseTexture", 0);
	renderPlane(program, projection, view, _camera.getAttr().pos);
	renderer()->setRenderTarget(nullptr);
}
```

`renderLight`（延迟光照，绑定 gBuffer 3 纹理到 unit 0/1/2，画 quad）：

```cpp
void GLDeferApp::renderLight(std::shared_ptr<rhi::IPipeline>& program, const glm::mat4 &projection, const glm::mat4 &view) {
	renderer()->setPipeline(program);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(0), 0); program->setUniform("gPosition", 0);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(1), 1); program->setUniform("gNormal", 1);
	renderer()->bindTexture(m_gBuffer->colorTexture2D(2), 2); program->setUniform("gAlbedoSpec", 2);
	program->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
	const float linear = 0.7f, quadratic = 1.8f, constant = 1.0f;
	program->setUniform("enableVolume", m_enableVolume);
	auto lightPositionsColor = GetLightPosAndColors(m_Count, 2);
	for (unsigned int i = 0; i < lightPositionsColor.size(); i++) {
		program->setUniform("lights[" + std::to_string(i) + "].Position",
		                    glm::value_ptr(lightPositionsColor[i].first), 1, 3);
		program->setUniform("lights[" + std::to_string(i) + "].Color",
		                    glm::value_ptr(lightPositionsColor[i].second), 1, 3);
		program->setUniform("lights[" + std::to_string(i) + "].Linear", linear);
		program->setUniform("lights[" + std::to_string(i) + "].Quadratic", quadratic);
		const float maxBrightness = std::fmaxf(std::fmaxf(lightPositionsColor[i].second.r, lightPositionsColor[i].second.g), lightPositionsColor[i].second.b);
		float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
		program->setUniform("lights[" + std::to_string(i) + "].Radius", radius);
	}
	renderer()->setVertexBuffer(m_quadVb);
	renderer()->draw(m_quadVertexCount, 0);
}
```

`drawScene` 迁移（含**深度-only blit**）：

```cpp
void GLDeferApp::drawScene(const float dt) {
	GLCameraBaseApp::drawScene(dt);
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();
	renderGBuffer(m_gBufferProgram, projection, view);
	renderLight(m_lightProgram, projection, view);

	// 深度-only blit：把 GBuffer 深度拷贝到默认 FBO，供 lightbox 深度测试
	renderer()->blitFramebuffer(m_gBuffer, nullptr, rhi::BlitMask::Depth);

	renderLightBox(m_lightBoxProgram, projection, view);

	ImGui::Begin("OpenGL");
	ImGui::SliderInt("Cube Count", &m_Count, 1, 13);
	ImGui::Checkbox("Enable Volume", &m_enableVolume);
	ImGui::End();
}
```

> 注意：`renderLightBox` 画代表光源的小立方体（依赖 blit 进默认 FBO 的深度做正确遮挡）。`renderOneCube`/`renderCubes`/`renderPlane` 的 buffer 挂接方式同 Bloom。

- [ ] **Step 3: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。若 `setRenderTarget` 对 `IPipeline` 引用（`renderLightBox` 等参数用 `std::shared_ptr<IPipeline>&`）有 const 问题，改用按值或 `std::shared_ptr<rhi::IPipeline>`。

- [ ] **Step 4: 回归 + 单跑 Defer**

Run: `./scripts/run.sh all -b gl -d 1`，Expected 46/46 OK。
Run: `./scripts/run.sh all -a Defer -b gl -d 1`，Expected [ OK ] Defer。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Light/Advanced/GLDeferApp.hpp src/app/GL/Light/Advanced/GLDeferApp.cpp
git commit -m "refactor(app): migrate Defer to RHI (GBuffer MRT + depth-only blit)"
```

---

## Self-Review 备注

- **Spec 覆盖**：T1 覆盖 §2.1（BlitMask）、§2.2b（bindTexture raw）、§2.2（attachment filter/wrap）；T2/T3/T4 覆盖 §3.1/§3.2/§3.3；测试红线覆盖 §5。✓
- **T1 关键实现注意**：`FramebufferAttachment` 聚合初始化时，Bloom/Defer 需显式传 filter/wrap（Defer 3 个附件 Nearest）。若聚合顺序与新字段不匹配，编译期会报错，届时按 `{type,format,external,samples,minFilter,magFilter,wrapS,wrapT}` 顺序调整。
- **布局传递**：三个 App 的 `compileShader` 需要 cube/quad 的 `VertexLayout` 来 `createPipeline`。确保在创建 buffer 时保留 `Geometry::layout` 传给 `compileShader(layout...)`。
- **raw 绑定**：后处理采样用 `renderer()->bindTexture(rt->colorTexture2D(n), unit)`（T1 raw 重载）。Hdr 也可用持有的 `_colorBuffer`（shared_ptr 别名，no-op deleter）。
- **类型一致性**：`blitFramebuffer` 第三个参数类型为 `rhi::BlitMask`；Defer 传 `rhi::BlitMask::Depth`。`FramebufferAttachment` 新字段默认 Linear/ClampToEdge 与现 GLRenderTarget 行为一致（颜色 Linear、wrap 默认；仅深度固定 Nearest/ClampToEdge）。✓
