# RHI Migration Phase C — Advanced Forward (BlinnPhong/Gamma/NormalMap/ParallaxMap) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 4 个 Light/Advanced forward App（BlinnPhong、Gamma、NormalMap、ParallaxMap）从原生 GLProgram 迁移到 RHI，并扩展 RhiGeometry 支持手写交错顶点数组。

**Architecture:** 复用计划B 确立的 `RhiGeometry::Create` + 双管线模板。BlinnPhong/Gamma 的 Plane 物体用现有 Create（默认布局 pos0/color1/uv2/normal3，drawArrays）；光源 Sphere 用 `Create(shape,false,false,true)` indexed。NormalMap/ParallaxMap 手写交错数组（pos/normal/uv/tangent/bitangent，14 float/顶点）由新增 `RhiGeometry::CreateFromArray` 入口处理，生成单 VBO + 自定义 VertexLayout。

**Tech Stack:** C++17、RHI 抽象层（IRenderer/IPipeline/IShader/ITexture2D/IBuffer）、glm、imgui。

## Global Constraints

- **红线：** 每任务 `./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1` 必须 46/46 OK。
- **不重命名** 类名/AppType；只改本任务涉及文件。
- 禁用 GLProgram/glad/GLImageTexture2D include；用 `renderer()` RHI 调用。
- `PROGRESS.md` 更新到本地（gitignored，不入库）；代码提交链 `docs` 先行。
- shader 路径：`join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "<Dir>")`；shader 为 `.vs/.fs` 扩展名。
- vec4 用 `setUniform(name, glm::value_ptr(v), 1, 4)`；vec3 用 `setUniform(name, glm::value_ptr(v), 1, 3)`；bool 用 bool 重载；int 用 int 重载；矩阵用 `setUniform(name, glm::value_ptr(m), 1)`。
- 纹理 `RhiImage::Load2D(renderer().get(), file)`；绑定 `renderer()->bindTexture(tex, unit)`。

---

### Task 1: RhiGeometry 新增手写交错数组入口 `CreateFromArray`

**Files:**
- Modify: `src/app/GL/RhiGeometry.hpp`（新增函数声明）
- Modify: `src/app/GL/RhiGeometry.cpp`（新增实现）
- Test: 无独立测试文件，用现有 App 编译 + 运行回归验证

**Interfaces:**
- Consumes: `rhi::IRenderer*`、`rhi::IBuffer`、`rhi::VertexLayout`、`rhi::VertexElement`（均已在 RhiGeometry.hpp 依赖中）。
- Produces: 新函数 `Geometry CreateFromArray(rhi::IRenderer* renderer, const float* data, size_t byteSize, uint32_t vertexCount, const rhi::VertexLayout& layout)`。返回的 `Geometry` 只设置 `vertexBuffer`（binding 0，单 VBO 交错）与 `layout`、`vertexCount`；不建 uvBuffer/normalBuffer/indexBuffer（手写数组全交错在单 buffer，用 `draw(vertexCount,0)`）。

- [ ] **Step 1: 在 `RhiGeometry.hpp` 添加声明**

在 `Create` 声明后添加：

```cpp
// 上传一个手写交错顶点数组到 RHI 缓冲（单 VBO，binding 0），并直接使用调用方提供的 VertexLayout。
// 用于 NormalMap/ParallaxMap 等非 Shape 几何（pos/normal/uv/tangent/bitangent 交错，glDrawArrays）。
// 不创建 uv/normal/index buffer；绘制用 vertexCount + renderer()->draw(vertexCount, 0)。
Geometry CreateFromArray(rhi::IRenderer* renderer, const float* data, size_t byteSize,
                         uint32_t vertexCount, const rhi::VertexLayout& layout);
```

- [ ] **Step 2: 在 `RhiGeometry.cpp` 添加实现**

```cpp
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
```

- [ ] **Step 3: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过，无新增 error/warning。

- [ ] **Step 4: 回归确认现有 Create 未破坏**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/RhiGeometry.hpp src/app/GL/RhiGeometry.cpp
git commit -m "feat(rhi): add RhiGeometry::CreateFromArray for interleaved vertex arrays"
```

---

### Task 2: 迁移 BlinnPhong + Gamma

**Files:**
- Modify: `src/app/GL/Light/Advanced/GLBlinnPhongApp.hpp`、`GLBlinnPhongApp.cpp`
- Modify: `src/app/GL/Light/Advanced/GLGammaApp.hpp`、`GLGammaApp.cpp`

**Interfaces:**
- Consumes: `RhiGeometry::Create`（默认布局，uv=2/normal=3）；`RhiGeometry::CreateFromArray`（本任务不用，Task 3 用）。
- Produces: 两 App 的 RHI 化迁移。每个 App 的 initApp 内联创建 2 条管线（光源 Sphere drawIndexed + 物体 Plane drawArrays）。依赖成员：光源 `_vb`/`_ebo`/`_indexCount`、物体 `_planeVb`/`_planeUv`/`_planeNormal`/`_planeVertexCount`、`_targetPipeline`/`_lightPipeline`。

- [ ] **Step 1: 重写 `GLBlinnPhongApp.hpp`**

将 `#include "native/GL/GLProgram.hpp"` 移除，替换为：

```cpp
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"

class GLImageTexture2D;
class GLBlinnPhongApp : public GLCameraBaseApp {
public:
	virtual ~GLBlinnPhongApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	Sphere shape{};
	std::shared_ptr<rhi::IPipeline> _targetPipeline{};
	std::shared_ptr<rhi::IPipeline> _lightPipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _ebo{};
	uint32_t _indexCount{0};
	std::shared_ptr<rhi::IBuffer> _planeVb{};
	std::shared_ptr<rhi::IBuffer> _planeUv{};
	std::shared_ptr<rhi::IBuffer> _planeNormal{};
	uint32_t _planeVertexCount{0};
	bool _enableBlinnPhong{};
	float _curTime{};
	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
	std::shared_ptr<rhi::ITexture2D> _texture{};
};
```

- [ ] **Step 2: 重写 `GLBlinnPhongApp.cpp`（RHI 化）**

替换文件内容为：

```cpp
#include "GLBlinnPhongApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "app/GL/RhiGeometry.hpp"
#include "app/GL/RhiImage.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "base/Log.hpp"
#include "imgui.h"
#include "geometry/Plane.hpp"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLBlinnPhongApp::~GLBlinnPhongApp() {
}

bool GLBlinnPhongApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");

	// 光源管线（Sphere shape，pos+color indexed；layout 来自 Create 返回值）
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Source.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Source.fs");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		auto geo = RhiGeometry::Create(renderer().get(), shape, false, false, true);
		_vb = geo.vertexBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_lightPipeline = renderer()->createPipeline(geo.layout, shader);
	}

	// 物体管线（Plane shape，pos+color+uv+normal，drawArrays；默认布局 uv=2/normal=3）
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Advanced", "BlinnPhong", "Object.vs");
		const auto ffile = join(shaderDir, "Advanced", "BlinnPhong", "Object.fs");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		Plane plane{};
		auto geo = RhiGeometry::Create(renderer().get(), plane, true, true, false);
		_planeVb = geo.vertexBuffer;
		_planeUv = geo.uvBuffer;
		_planeNormal = geo.normalBuffer;
		_planeVertexCount = geo.vertexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
	}

	{
		const auto imgFile = join(StaticCollector::getImagePath(), "wood.png");
		_texture = RhiImage::Load2D(renderer().get(), imgFile);
		ExitIfFailed(_texture != nullptr, "Failed to load texture from file {}", imgFile);
	}
	return true;
}

void GLBlinnPhongApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::Checkbox("Enable Blinn Phong", &_enableBlinnPhong);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	const auto lightPos = glm::vec3(-0.0f, 0.0f, 0.f);
	//draw object（Plane，drawArrays）
	{
		renderer()->bindTexture(_texture, 0);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(10.f));
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_planeVb);
		renderer()->setVertexBuffer(_planeUv, 1);
		renderer()->setVertexBuffer(_planeNormal, 2);
		_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		_targetPipeline->setUniform("textureSampler", 0);
		_targetPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		_targetPipeline->setUniform("lightPos", glm::value_ptr(lightPos), 1, 3);
		_targetPipeline->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
		_targetPipeline->setUniform("enableBlinnPhong", _enableBlinnPhong);
		renderer()->draw(_planeVertexCount, 0);
	}

	//draw light source（Sphere，drawIndexed）
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
		_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
		_lightPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	return GLApp::drawScene(dt);
}
```

> 注：原文件用 `createVertexBuffer()`/`createPlaneBuffer()` 建 GL 句柄；RHI 化后这些辅助函数不再需要，geometry + pipeline 全部在 initApp 内联创建（遵循 GLSimpleLightSpecular.cpp 模式）。hpp 中也应删除 `createVertexBuffer`/`createPlaneBuffer` 声明。

- [ ] **Step 3: 按同一模式重写 `GLGammaApp.hpp` + `GLGammaApp.cpp`**

hpp 与 BlinnPhong 结构相同，差异仅在成员与 ImGui：

hpp 成员改为：
```cpp
	bool _enableGamma{};
	float _gammaValue{ 2.2 };
```
其余（pipeline/_vb/_ebo/_plane*/_texture/_lightColor）与 BlinnPhong 一致。

cpp 差异（相对 BlinnPhong）：
- initApp 结构完全同 BlinnPhong（光源 Sphere 块 + 物体 Plane 块 + 纹理块），仅 shader 路径改为 `Advanced/Gamma` 下的 `Source.vs/.fs`、`Object.vs/.fs`，其余（geometry Create、成员赋值、createPipeline）逐字一致。
- shader 目录 `Advanced/Gamma`；文件 `Source.vs/.fs`、`Object.vs/.fs`。
- 物体管线 uniform 增补，shader 中数组用逐元素或 value_ptr 上传：

```cpp
	std::vector<glm::vec3> lightPoses = {
		glm::vec3(-2.0f, 0.0f, 0.f),
		glm::vec3(-1.0f, 0.0f, 0.f),
		glm::vec3(-0.0f, 0.0f, 0.f),
		glm::vec3(1.0f, 0.0f, 0.f),
		glm::vec3(2.0f, 0.0f, 0.f)
	};
	std::vector<glm::vec3> lightColors = {
		glm::vec3(1, 0, 0),
		glm::vec3(0, 1, 0),
		glm::vec3(1, 1, 1),
		glm::vec3(0, 0, 1),
		glm::vec3(1, 1, 0)
	};
```

物体 draw 块（Plane，drawArrays）：
```cpp
		renderer()->bindTexture(_texture, 0);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::scale(model, glm::vec3(10.f));
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_planeVb);
		renderer()->setVertexBuffer(_planeUv, 1);
		renderer()->setVertexBuffer(_planeNormal, 2);
		_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		_targetPipeline->setUniform("textureSampler", 0);
		_targetPipeline->setUniform("lightColor", glm::value_ptr(_lightColor), 1, 4);
		_targetPipeline->setUniform("lightPositions", glm::value_ptr(lightPoses[0]), static_cast<int>(lightPoses.size()), 3);
		_targetPipeline->setUniform("lightColors", glm::value_ptr(lightColors[0]), static_cast<int>(lightColors.size()), 3);
		_targetPipeline->setUniform("viewPos", glm::value_ptr(_camera.getAttr().pos), 1, 3);
		_targetPipeline->setUniform("enableGamma", _enableGamma);
		_targetPipeline->setUniform("gammaValue", _gammaValue);
		renderer()->draw(_planeVertexCount, 0);
```

光源 draw 块（Sphere，drawIndexed）：与 BlinnPhong 相同，但循环 5 个光源、`lightColor` 取 `glm::vec4(lightColors[i], 1.0)`：

```cpp
	for (auto i = 0; i < 5; i++) {
		const auto lightPos = lightPoses[i];
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::scale(model, glm::vec3(0.05, 0.05, 0.05));

		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
		_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
		const glm::vec4 lc(lightColors[i], 1.0f);
		_lightPipeline->setUniform("lightColor", glm::value_ptr(lc), 1, 4);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
```

> 注：Gamma Object.fs 声明 `enableBlinnPhong` 但原 cpp **不设置它**（保持 GL 默认 false）。RHI 化时也**不设置**，保持行为一致。

- [ ] **Step 5: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 6: 回归 + 视觉比对**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。
额外：单独运行 BlinnPhongApp 验证 "Enable Blinn Phong" 开关效果；GammaApp 验证 "Enable Gamma"/"Gamma Value" 滑条。

- [ ] **Step 7: 提交**

```bash
git add src/app/GL/Light/Advanced/GLBlinnPhongApp.hpp src/app/GL/Light/Advanced/GLBlinnPhongApp.cpp src/app/GL/Light/Advanced/GLGammaApp.hpp src/app/GL/Light/Advanced/GLGammaApp.cpp
git commit -m "refactor(app): migrate BlinnPhong/Gamma to RHI"
```

---

### Task 3: 迁移 NormalMap + ParallaxMap

**Files:**
- Modify: `src/app/GL/Light/Advanced/GLNormalMapApp.hpp`、`GLNormalMapApp.cpp`
- Modify: `src/app/GL/Light/Advanced/GLParallaxMapApp.hpp`、`GLParallaxMapApp.cpp`

**Interfaces:**
- Consumes: `RhiGeometry::CreateFromArray`（Task 1）、`RhiImage::Load2D`、`rhi::ITexture2D`。
- Produces: 两 App 的 RHI 化迁移。依赖成员 `_vb`（单交错 VBO）、`_vertexCount`（6）、`_pipeline`、三纹理成员。

- [ ] **Step 1: 重写 `GLNormalMapApp.hpp`**

移除 `#include "native/GL/GLProgram.hpp"`，替换为 RHI includes；`_program` 改 `_pipeline`；`_vao`/`_vbo` 改 `_vb`/`_vertexCount`；纹理改 `rhi::ITexture2D`：

```cpp
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IShader.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"
#include "geometry/Cube.hpp"

class GLNormalMapApp : public GLCameraBaseApp {
public:
	virtual ~GLNormalMapApp();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	void compileShader();
	void createTextures();

private:
	std::shared_ptr<rhi::IPipeline> _pipeline{};
	std::shared_ptr<rhi::IShader> _shader{};
	uint32_t _vertexCount{0};
	std::shared_ptr<rhi::IBuffer> _vb{};
	bool _enableNormalMap{};
	std::shared_ptr<rhi::ITexture2D> _brick{};
	std::shared_ptr<rhi::ITexture2D> _brickNormal{};
};
```

- [ ] **Step 2: 重写 `GLNormalMapApp.cpp`**

将 `CreateRectBuffer()` 的硬编码交错数组抽出，改为返回 `Geometry`（用 `CreateFromArray`）：

```cpp
#include "GLNormalMapApp.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
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
#include "geometry/Rect.hpp"
#include "base/Constexpr.hpp"

using namespace Constexpr;
using FileUtils::join;
using namespace ErrorHandle;

GLNormalMapApp::~GLNormalMapApp() {
}

static RhiGeometry::Geometry CreateRectBuffer(rhi::IRenderer* renderer) {
	glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
	glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
	glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
	glm::vec3 pos4( 1.0f,  1.0f, 0.0f);
	glm::vec2 uv1(0.0f, 1.0f);
	glm::vec2 uv2(0.0f, 0.0f);
	glm::vec2 uv3(1.0f, 0.0f);
	glm::vec2 uv4(1.0f, 1.0f);
	glm::vec3 nm(0.0f, 0.0f, 1.0f);

	glm::vec3 tangent1, bitangent1;
	glm::vec3 tangent2, bitangent2;
	glm::vec3 edge1 = pos2 - pos1;
	glm::vec3 edge2 = pos3 - pos1;
	glm::vec2 deltaUV1 = uv2 - uv1;
	glm::vec2 deltaUV2 = uv3 - uv1;

	float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
	bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
	bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

	edge1 = pos3 - pos1;
	edge2 = pos4 - pos1;
	deltaUV1 = uv3 - uv1;
	deltaUV2 = uv4 - uv1;
	f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
	bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
	bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

	float quadVertices[] = {
		pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
		pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
		pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
		pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z
	};

	constexpr int stride = 14 * static_cast<int>(sizeof(float));
	rhi::VertexLayout layout;
	layout.elements.push_back({rhi::VertexElement::Float3, 0, 0, rhi::VertexInputRate::PerVertex, 0, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 1, 0, rhi::VertexInputRate::PerVertex, 12, stride});
	layout.elements.push_back({rhi::VertexElement::Float2, 2, 0, rhi::VertexInputRate::PerVertex, 24, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 3, 0, rhi::VertexInputRate::PerVertex, 32, stride});
	layout.elements.push_back({rhi::VertexElement::Float3, 4, 0, rhi::VertexInputRate::PerVertex, 44, stride});

	return RhiGeometry::CreateFromArray(renderer, quadVertices, sizeof(quadVertices), 6, layout);
}

bool GLNormalMapApp::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	createTextures();
	compileShader();

	auto geo = CreateRectBuffer(renderer().get());
	_vb = geo.vertexBuffer;
	_vertexCount = geo.vertexCount;
	_pipeline = renderer()->createPipeline(geo.layout, _shader);
	_pipeline->setDepthTest(true);
	return true;
}

static std::shared_ptr<rhi::ITexture2D> CreateTexture(rhi::IRenderer* renderer, const std::string& imgname) {
	const auto resDir = StaticCollector::getImagePath();
	const auto imgFile = join(resDir, imgname);
	auto texture = RhiImage::Load2D(renderer, imgFile);
	ExitIfFailed(texture != nullptr, "Failed to load texture from file {}", imgFile);
	return texture;
}

void GLNormalMapApp::createTextures() {
	_brick = CreateTexture(renderer().get(), "brickwall.jpg");
	_brickNormal = CreateTexture(renderer().get(), "brickwall_normal.jpg");
}

void GLNormalMapApp::compileShader() {
	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light", "Advanced", "NormalMap");
	_shader = renderer()->createShader();
	const auto vfile = join(shaderDir, "NormalMap.vs");
	const auto ffile = join(shaderDir, "NormalMap.fs");
	auto ok = _shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
	                             {rhi::ShaderStage::Fragment, ffile, "main", false} });
	ExitIfFailed(ok, "Create RHI shader failed: {}", _shader->getLog());
}

static void RenderRect(rhi::IRenderer* renderer, rhi::IPipeline* pipeline,
                       const std::shared_ptr<rhi::IBuffer>& vb, uint32_t count) {
	renderer->setPipeline(pipeline);
	renderer->setVertexBuffer(vb);
	renderer->draw(count, 0);
}

void GLNormalMapApp::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Checkbox("Enable Normal Map", &_enableNormalMap);
	ImGui::End();

	glm::vec3 lightPos(0.0f, 0.0f, 1.0f);
	const auto attr = _camera.getAttr();
	glm::mat4 projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	glm::mat4 view = _camera.getViewMatrix();
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	_pipeline->setUniform("diffuseMap", 0);
	_pipeline->setUniform("normalMap", 1);
	_pipeline->setUniform("projection", glm::value_ptr(projection), 1);
	_pipeline->setUniform("view", glm::value_ptr(view), 1);
	glm::mat4 model = glm::mat4(1.0f);
	_pipeline->setUniform("model", glm::value_ptr(model), 1);
	_pipeline->setUniform("viewPos", glm::value_ptr(attr.pos), 1, 3);
	_pipeline->setUniform("lightPos", glm::value_ptr(lightPos), 1, 3);
	_pipeline->setUniform("enableNM", _enableNormalMap ? 1 : 0);
	renderer()->bindTexture(_brick, 0);
	renderer()->bindTexture(_brickNormal, 1);
	renderer()->draw(_vertexCount, 0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, lightPos);
	model = glm::scale(model, glm::vec3(0.1f));
	model = glm::translate(model, glm::vec3(10.0, 10.0, 0.0));
	_pipeline->setUniform("model", glm::value_ptr(model), 1);
	renderer()->draw(_vertexCount, 0);

	return GLApp::drawScene(dt);
}
```

> 说明：`enableNM` 是 shader 中 `uniform int enableNM`（**int 非 bool**），故用 `_enableNormalMap ? 1 : 0`。原代码没有独立光源管线（同一 shader 绘制两次 quad），故单管线即可。需要 `_shader` 成员（hpp 中加 `std::shared_ptr<rhi::IShader> _shader{};`）以便 initApp 建管线。

- [ ] **Step 3: ParallaxMap.hpp 同构重写**

与 NormalMap.hpp 同构（含 `#include "rhi/core/IShader.hpp"` 与 `std::shared_ptr<rhi::IShader> _shader{};`）。差异：成员加 `_brickDisp`；bool 三个：`_enableDisp`/`_enableSteep`/`_enableOcclusion`；float `_heightScale`：

```cpp
	std::shared_ptr<rhi::ITexture2D> _brick{};
	std::shared_ptr<rhi::ITexture2D> _brickNormal{};
	std::shared_ptr<rhi::ITexture2D> _brickDisp{};
	bool _enableDisp{};
	bool _enableSteep{};
	bool _enableOcclusion{};
	float _heightScale{ 0.1f };
```

- [ ] **Step 4: ParallaxMap.cpp 同构重写**

与 NormalMap 几乎相同，差异：
- `CreateRectBuffer` 代码完全相同（两个 App 用同一份），可直接复制。
- shader 目录 `Advanced/ParallaxMap`；文件 `ParallaxMap.vs/.fs`。
- 纹理三张：`bricks2.jpg` → `_brick`、`bricks2_normal.jpg` → `_brickNormal`、`bricks2_disp.jpg` → `_brickDisp`。
- uniform：多 `depthMap`=2、`heightScale`、`enableDisp`/`enableSteep`/`enableOcclusion`（bool）。
- ImGui 面板：`Enable Normal Map`/`Enable Steep`/`Enable Occlusion` Checkbox + `Height Scale` InputFloat。
- draw 块：

```cpp
	renderer()->setPipeline(_pipeline);
	renderer()->setVertexBuffer(_vb);
	_pipeline->setUniform("diffuseMap", 0);
	_pipeline->setUniform("normalMap", 1);
	_pipeline->setUniform("depthMap", 2);
	_pipeline->setUniform("projection", glm::value_ptr(projection), 1);
	_pipeline->setUniform("view", glm::value_ptr(view), 1);
	glm::mat4 model = glm::mat4(1.0f);
	_pipeline->setUniform("model", glm::value_ptr(model), 1);
	_pipeline->setUniform("viewPos", glm::value_ptr(attr.pos), 1, 3);
	_pipeline->setUniform("lightPos", glm::value_ptr(lightPos), 1, 3);
	_pipeline->setUniform("heightScale", _heightScale);
	_pipeline->setUniform("enableDisp", _enableDisp);
	_pipeline->setUniform("enableSteep", _enableSteep);
	_pipeline->setUniform("enableOcclusion", _enableOcclusion);
	renderer()->bindTexture(_brick, 0);
	renderer()->bindTexture(_brickNormal, 1);
	renderer()->bindTexture(_brickDisp, 2);
	renderer()->draw(_vertexCount, 0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, lightPos);
	model = glm::scale(model, glm::vec3(0.1f));
	_pipeline->setUniform("model", glm::value_ptr(model), 1);
	renderer()->draw(_vertexCount, 0);
```

（`lightPos` 在 ParallaxMap 为 `glm::vec3(1.0f, 1.0f, 1.0f)`。）

- [ ] **Step 5: 编译**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 6: 回归 + 视觉比对**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。
额外：NormalMapApp 验证 "Enable Normal Map" 开关；ParallaxMapApp 验证 Height Scale 与三个开关。

- [ ] **Step 7: 提交**

```bash
git add src/app/GL/Light/Advanced/GLNormalMapApp.hpp src/app/GL/Light/Advanced/GLNormalMapApp.cpp src/app/GL/Light/Advanced/GLParallaxMapApp.hpp src/app/GL/Light/Advanced/GLParallaxMapApp.cpp
git commit -m "refactor(app): migrate NormalMap/ParallaxMap to RHI"
```

---

### 最终验证（3 任务全部完成后）

- [ ] **Step: 全量构建 + 回归**

Run: `./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

- [ ] **Step: 更新 PROGRESS.md**

将计划C 三任务、技术债、提交链记录到 `PROGRESS.md`（本地 gitignored）。
