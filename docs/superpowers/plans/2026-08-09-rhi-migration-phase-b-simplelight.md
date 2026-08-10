# 计划B：SimpleLight 9 App 迁移到 RHI 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 Light 目录 9 个 SimpleLight App（Ambination/Diffuse/Specular/Material/Map + LightSource Direction/Point/Spot/Mult）从 native GL（GLProgram/glad/GLImageTexture2D）迁移到 RHI。

**Architecture:** 扩展 `RhiGeometry::Create` 支持自定义 normal/uv 的 location（semantic），使 SimpleLight 的 shader 布局（normal=2, uv=3）与 RHI 模板匹配；随后把 9 个 App 的 raw VAO/VBO/GLProgram 改为 RhiGeometry + IPipeline/IBuffer/ITexture2D。

**Tech Stack:** C++17、RHI 接口层（IRenderer/IPipeline/IShader/IBuffer/ITexture2D）、glm、vcpkg。

## Global Constraints

- 不重命名任何 App 类名（去 GL 前缀留到收尾计划）。
- 只改计划指定文件，不越界。
- 每任务红线：`./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1` 必须 46/46 OK。
- RhiGeometry 默认布局（uv=2, normal=3）保持不变，不得破坏已迁移的 Cube/Camera/LoadModel。
- 渲染结果须与原实现一致（光源位置、颜色拾取器、材质交互正常）。
- 每任务结束提交一个 commit。

---

### Task 1: RhiGeometry::Create 支持自定义 location

**Files:**
- Modify: `src/app/GL/RhiGeometry.hpp:20-21`
- Modify: `src/app/GL/RhiGeometry.cpp:8-45`

**Interfaces:**
- Consumes: `rhi::VertexElement{format, semantic, binding, inputRate, offset, stride}`（`src/rhi/core/Common.hpp:26-33`），`semantic` 即 GL location。
- Produces: `RhiGeometry::Layout{int uvLocation=2; int normalLocation=3;}` 结构；`RhiGeometry::Create(renderer, shape, useUv, useNormal, useIndex, const Layout& layout = {})`——追加可选第 5 参，默认 uv=2/normal=3，SimpleLight 传 `{.uvLocation=3, .normalLocation=2}`。

- [ ] **Step 1: 扩展 `RhiGeometry.hpp` 增加 Layout 结构与可选参数**

```cpp
namespace RhiGeometry {
// 上传一个 Shape 到 RHI 缓冲，并生成对应的 VertexLayout。
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

// 可选布局参数：指定 uv / normal 的 location（GL semantic）。
// 默认 uv=2、normal=3（与已迁移模板一致）；SimpleLight 系列用 uv=3、normal=2。
struct Layout {
    int uvLocation{2};
    int normalLocation{3};
};

Geometry Create(rhi::IRenderer* renderer, Shape& shape,
                bool useUv, bool useNormal, bool useIndex,
                const Layout& layout = {});
}
```

- [ ] **Step 2: 修改 `RhiGeometry.cpp` 按 layout 生成 location**

```cpp
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

} // namespace RhiGeometry
```

- [ ] **Step 3: 构建验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

- [ ] **Step 4: 回归验证已迁移 App（默认布局未变）**

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK（Cube/Camera/LoadModel 用默认 uv=2/normal=3 不受影响）。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/RhiGeometry.hpp src/app/GL/RhiGeometry.cpp
git commit -m "feat(app): support custom uv/normal location in RhiGeometry::Create"
```

---

### Task 2: 无纹理 SimpleLight 迁移（Ambination/Diffuse/Specular/Material）

**Files:**
- Modify: `src/app/GL/Light/GLSimpleLightAmbination.hpp/.cpp`
- Modify: `src/app/GL/Light/GLSimpleLightDiffuse.hpp/.cpp`
- Modify: `src/app/GL/Light/GLSimpleLightSpecular.hpp/.cpp`
- Modify: `src/app/GL/Light/GLSimpleLightMaterial.hpp/.cpp`

**Interfaces:**
- Consumes: `RhiGeometry::Create(renderer, shape, useUv, useNormal, useIndex, {.normalLocation=2})`；`RhiImage::Load2D`（T3 用，本任务不用纹理）；`renderer()->createShader/createPipeline/setPipeline/setVertexBuffer/draw`；`pipeline->setDepthTest/setUniform`。
- Produces: 4 个无纹理 App 全部 RHI 化，验证「光源+物体双管线 + Sphere + normal=2」模板。

每个 App 结构相同（以 Diffuse 为准，其余仅 shader 路径/成员命名/绘制逻辑不同）。以下以 GLSimpleLightDiffuse 给出完整实现，Ambination/Specular/Material 参照同样模式（shader 目录：Ambination/Source+Object、Diffuse/Light+Object、Specular/Light+Object、Material/Light+Object）。

- [ ] **Step 1: 改写 `GLSimpleLightDiffuse.hpp`**

```cpp
#pragma once
#include "app/GL/Base/GLCameraBaseApp.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include <memory>
#include <array>
#include "geometry/Camera.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Sphere.hpp"

class GLSimpleLightDiffuse : public GLCameraBaseApp {

public:
	virtual ~GLSimpleLightDiffuse();

protected:
	virtual bool initApp() override;
	virtual void drawScene(const float dt);

private:
	Sphere shape{};
	std::shared_ptr<rhi::IPipeline> _targetPipeline{};
	std::shared_ptr<rhi::IPipeline> _lightPipeline{};
	std::shared_ptr<rhi::IBuffer> _vb{};
	std::shared_ptr<rhi::IBuffer> _normal{};
	std::shared_ptr<rhi::IBuffer> _ebo{};
	uint32_t _indexCount{0};

	glm::vec4 _lightColor{1.0, 1.0, 1.0, 1.0};
};
```

- [ ] **Step 2: 改写 `GLSimpleLightDiffuse.cpp`**

```cpp
#include "GLSimpleLightDiffuse.hpp"
#include "base/StaticCollector.hpp"
#include "base/ErrorHandle.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "app/GL/RhiGeometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include <utils/FileUtils.hpp>
using FileUtils::join;
using namespace ErrorHandle;

GLSimpleLightDiffuse::~GLSimpleLightDiffuse() {
}

bool GLSimpleLightDiffuse::initApp() {
	if (!GLCameraBaseApp::initApp()) {
		return false;
	}

	const auto shaderDir = join(StaticCollector::getGLShaderPath(), "Light");
	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Diffuse", "Light.vert");
		const auto ffile = join(shaderDir, "Diffuse", "Light.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());
		_lightPipeline = renderer()->createPipeline(renderer()->getDummyLayout(), shader);
	}

	{
		auto shader = renderer()->createShader();
		const auto vfile = join(shaderDir, "Diffuse", "Object.vert");
		const auto ffile = join(shaderDir, "Diffuse", "Object.frag");
		auto ok = shader->compile({ {rhi::ShaderStage::Vertex, vfile, "main", false},
		                            {rhi::ShaderStage::Fragment, ffile, "main", false} });
		ExitIfFailed(ok, "Create RHI shader failed: {}", shader->getLog());

		auto geo = RhiGeometry::Create(renderer().get(), shape, false, true, true, {.normalLocation = 2});
		_vb = geo.vertexBuffer;
		_normal = geo.normalBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
	}
	return true;
}

void GLSimpleLightDiffuse::drawScene(const float dt) {
	GLApp::drawScene(dt);
	ImGui::Begin("OpenGL");
	ImGui::Text("Color Picker with Alpha:");
	ImGui::ColorEdit4("Color with Alpha", &_lightColor[0]);
	ImGui::End();

	static float curTime = 0;
	curTime += dt;
	const auto projection = glm::perspective(glm::radians(_camera.zoom()), aspectRatio(), 0.1f, 100.0f);
	const auto view = _camera.getViewMatrix();

	auto lightPos = glm::vec3(1.0f, 1.0f, 1.50f);
	//draw light source（仅 pos+color，用独立管线）
	{
		renderer()->setPipeline(_lightPipeline);
		renderer()->setVertexBuffer(_vb);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, lightPos);
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		model = glm::scale(model, glm::vec3(0.2, 0.2, 0.2));
		_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
		_lightPipeline->setUniform("lightColor", glm::value_ptr(&_lightColor), 1, 4);
		_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}

	//draw object（pos+color+normal，normal=2）
	{
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_normal, 2);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
		glm::vec4 objectColor(1.0f, 0.5f, 0.31f, 1.0f);
		glm::vec4 lightPos4(lightPos.x, lightPos.y, lightPos.z, 1.0f);
		_targetPipeline->setUniform("projection", glm::value_ptr(projection), 1);
		_targetPipeline->setUniform("view", glm::value_ptr(view), 1);
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		_targetPipeline->setUniform("lightColor", glm::value_ptr(&_lightColor), 1, 4);
		_targetPipeline->setUniform("objectColor", glm::value_ptr(&objectColor), 1, 4);
		_targetPipeline->setUniform("lightPos", glm::value_ptr(&lightPos4), 1, 4);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
}
```

> **setUniform 传 vec4 的正确写法**：先声明局部 `glm::vec4 v(...)` 变量，再 `setUniform(name, glm::value_ptr(&v), 1, 4)`。禁止内联 `new` / 三元临时对象（会泄漏或悬垂）。

- [ ] **Step 3: 确认 `setIndexBuffer` / `getDummyLayout` 存在，否则用 Renderer 等价 API**

Run: `grep -n "setIndexBuffer\|getDummyLayout" src/rhi/core/IRenderer.hpp src/rhi/core/IPipeline.hpp`
Expected: 确认精确签名。若 `setIndexBuffer` 不存在，改用 `renderer()->setIndexBuffer(_ebo)` 的既有方法名（参考 GLPipeline/GLRenderer 实现）；若 `getDummyLayout` 不存在，光源管线用 `renderer()->createPipeline({}, shader)` 传空 layout。

- [ ] **Step 4: 用同样的模式改写 Ambination/Specular/Material 的 .hpp/.cpp**

- Ambination shader：`Ambination/Source.vert/.frag`（光源）、`Ambination/Object.vert/.frag`（物体）。物体块与 Diffuse 相同（含 objectColor/lightPos）。
- Specular：`Specular/Light.vert/.frag`、`Specular/Object.vert/.frag`。物体块额外有 `setUniform("viewPos", ...)` 等，需照抄原 .cpp 的 uniform 调用并用 RHI 等价写法。
- Material：`Material/Light.vert/.frag`、`Material/Object.vert/.frag`。物体块含 `material.ambient/diffuse/specular/shininess` uniform（原实现可能用数组，照原 .cpp 翻译为对应 `setUniform`）。

对每个 App，逐一保留原 drawScene 的全部 uniform 与逻辑，仅把 `_xxxProgram.use()/update()` 换成对应 pipeline 的 `setUniform`，`glDrawElements` 换成 `setIndexBuffer + drawIndexed`。Ambination 注意：物体块在 InitApp 与 drawScene 中有无额外的 uniform 需照抄。

- [ ] **Step 5: 构建 + 运行验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

Run: 逐个运行 `./scripts/run.sh -a Ambination -b gl -d 3`、`-a Diffuse`、`-a Specular`、`-a Material`
Expected: 渲染正常，光源小球 + 物体，颜色拾取器可用。

- [ ] **Step 6: 提交**

```bash
git add src/app/GL/Light/GLSimpleLightAmbination.* src/app/GL/Light/GLSimpleLightDiffuse.* src/app/GL/Light/GLSimpleLightSpecular.* src/app/GL/Light/GLSimpleLightMaterial.*
git commit -m "refactor(app): migrate SimpleLight (Ambination/Diffuse/Specular/Material) to RHI"
```

---

### Task 3: SimpleLightMap 迁移（有纹理）

**Files:**
- Modify: `src/app/GL/Light/GLSimpleLightMap.hpp/.cpp`

**Interfaces:**
- Consumes: `RhiGeometry::Create(renderer, shape, useUv, useNormal, useIndex, {.uvLocation=3, .normalLocation=2})`；`RhiImage::Load2D(renderer, path)`；`renderer()->bindTexture(tex, unit)`；`pipeline->setUniform(name, int)`（采样器 int）；`pipeline->setUniform(name, glm::value_ptr(&v), 1, 4)`（vec4）。
- Produces: 有纹理 App 模板（Sphere + diffuse/specular 双纹理 + uv=3/normal=2）。

- [ ] **Step 1: 读原文件确认 uniform 与纹理绑定**

Run: `cat src/app/GL/Light/GLSimpleLightMap.cpp src/app/GL/Light/GLSimpleLightMap.hpp`
Expected: 记录所有 `_targetProgram.update(...)` 与 `texture()->bind(n)` 及成员。典型包括 `material.diffuse`（采样器，int）、`material.specular`（采样器，int）、`material.shininess`（int）、`material.ambient`/`material.diffuseColor`/`material.specularColor`（vec4）、`light.*`、`viewPos` 等，逐项翻译。

- [ ] **Step 2: 改写 `.hpp`**

参考 Task 2 的 Diffuse.hpp，但：
- 加 `std::shared_ptr<rhi::ITexture2D> _diffuseTex{};`、`std::shared_ptr<rhi::ITexture2D> _specularTex{};`（对应原 `GLImageTexture2D`）。
- include `rhi/core/ITexture2D.hpp`。
- 物体用 `RhiGeometry::Create(..., true, true, true, {.uvLocation=3, .normalLocation=2})`（useUv=true）。

- [ ] **Step 3: 改写 `.cpp`**

```cpp
	// initApp 中加载纹理
	const auto diffuseFile = join(StaticCollector::getImagePath(), "container2.jpg");
	_diffuseTex = RhiImage::Load2D(renderer().get(), diffuseFile);
	ExitIfFailed(_diffuseTex != nullptr, "Failed to load texture from file {}", diffuseFile);
	const auto specularFile = join(StaticCollector::getImagePath(), "container2_specular.jpg");
	_specularTex = RhiImage::Load2D(renderer().get(), specularFile);
	ExitIfFailed(_specularTex != nullptr, "Failed to load texture from file {}", specularFile);
```

物体管线创建：

```cpp
		auto geo = RhiGeometry::Create(renderer().get(), shape, true, true, true, {.uvLocation = 3, .normalLocation = 2});
		_vb = geo.vertexBuffer;
		_uv = geo.uvBuffer;
		_normal = geo.normalBuffer;
		_ebo = geo.indexBuffer;
		_indexCount = geo.indexCount;
		_targetPipeline = renderer()->createPipeline(geo.layout, shader);
		_targetPipeline->setDepthTest(true);
```

drawScene 物体块绑定纹理并设采样器：

```cpp
		renderer()->setPipeline(_targetPipeline);
		renderer()->setVertexBuffer(_vb);
		renderer()->setVertexBuffer(_uv, 1);
		renderer()->setVertexBuffer(_normal, 2);
		renderer()->bindTexture(_diffuseTex, 0);
		_targetPipeline->setUniform("material.diffuse", 0);
		renderer()->bindTexture(_specularTex, 1);
		_targetPipeline->setUniform("material.specular", 1);
		_targetPipeline->setUniform("material.shininess", 32);
		// 其余 uniform（lightColor/viewPos/objectColor/light.* 等）按原 .cpp 逐一翻译，
		// vec4 用 setUniform(name, glm::value_ptr(&v), 1, 4)，mat4 用 setUniform(name, glm::value_ptr(m), 1)
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
```

> **注意**：`material.shininess` 原实现可能传 float（如 `update("material.shininess", 32)`）或 int；若原实现传 float，用 `setUniform(name, (float)32)` 的 float 重载；若传 int 用 int 重载。核对原 .cpp 后再定。`material.diffuse`/`material.specular` 为 sampler（uniform sampler2D，int 值 0/1），用 int 重载。删除 `GLImageTexture2D` include，改用 `RhiImage`/`ITexture2D`。移除析构中 `glDelete*`。

- [ ] **Step 4: 构建 + 运行验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

Run: `./scripts/run.sh -a Map -b gl -d 3`（App 名可能为 `Map` 或 `SimpleLight_Map`，用 `run.sh all -a` 匹配，或查 GLAppFactory）
Expected: 渲染正常，双纹理贴图可见，材质拾取器可用。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Light/GLSimpleLightMap.*
git commit -m "refactor(app): migrate SimpleLightMap to RHI (dual texture)"
```

---

### Task 4: LightSource 4 个迁移（Direction/Point/Spot/Mult，Cube+双纹理+多实例）

**Files:**
- Modify: `src/app/GL/Light/LightSource/GLLightSourceDirection.hpp/.cpp`
- Modify: `src/app/GL/Light/LightSource/GLLightSourcePoint.hpp/.cpp`
- Modify: `src/app/GL/Light/LightSource/GLLightSourceSpot.hpp/.cpp`
- Modify: `src/app/GL/Light/LightSource/GLLightSourceMult.hpp/.cpp`

**Interfaces:**
- Consumes: `RhiGeometry::Create(renderer, Cube, true, true, true, {.uvLocation=3, .normalLocation=2})`（有纹理 Cube）；`RhiImage::Load2D`；`renderer()->bindTexture`；`pipeline->setUniform`（int/vec4/mat4）；`renderer()->drawIndexed`。
- Produces: 4 个有纹理 LightSource App RHI 化，验证「Cube + 双纹理 + 多实例循环 + 每实例 model uniform」模板。

- [ ] **Step 1: 读原文件确认成员与绘制循环**

Run: `cat src/app/GL/Light/LightSource/GLLightSourceDirection.hpp/.cpp`
Expected: 记录 `_object`（Cube）、双纹理、`cubePositions` 网格、每实例 `_targetProgram.update("model", model)` + `glDrawElements`。

- [ ] **Step 2: 改写 `.hpp`**

参考 Task 3 的 Map.hpp，`Sphere shape` 改为 `Cube _object{};`，保留双纹理成员与光源管线。include `geometry/Cube.hpp`。

- [ ] **Step 3: 改写 `.cpp`**

与 Task 3 相同的纹理加载 + 管线创建（`RhiGeometry::Create(renderer().get(), _object, true, true, true, {.uvLocation=3, .normalLocation=2})`）。

光源块（pos+color，用光源管线 + `setIndexBuffer` + `drawIndexed`）：

```cpp
	renderer()->setPipeline(_lightPipeline);
	renderer()->setVertexBuffer(_vb);
	_lightPipeline->setUniform("projection", glm::value_ptr(projection), 1);
	_lightPipeline->setUniform("view", glm::value_ptr(view), 1);
	glm::vec4 lc(_lightColor);
	_lightPipeline->setUniform("lightColor", glm::value_ptr(&lc), 1, 4);
	_lightPipeline->setUniform("model", glm::value_ptr(model), 1);
	renderer()->setIndexBuffer(_ebo);
	renderer()->drawIndexed(_indexCount, 0, 0);
```

物体块（双纹理 + 多实例循环，每实例 setUniform model + drawIndexed）：

```cpp
	renderer()->setPipeline(_targetPipeline);
	renderer()->setVertexBuffer(_vb);
	renderer()->setVertexBuffer(_uv, 1);
	renderer()->setVertexBuffer(_normal, 2);
	renderer()->bindTexture(_diffuseTex, 0);
	_targetPipeline->setUniform("material.diffuse", 0);
	renderer()->bindTexture(_specularTex, 1);
	_targetPipeline->setUniform("material.specular", 1);
	// 其余全局 uniform（light.direction/position/viewPos/material.shininess/light.ambient 等）在循环外设一次
	for (int i = 0; i < count; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, cubePositions[i]);
		float angle = 20.0f * curTime;
		model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
		_targetPipeline->setUniform("model", glm::value_ptr(model), 1);
		renderer()->setIndexBuffer(_ebo);
		renderer()->drawIndexed(_indexCount, 0, 0);
	}
```

> **注意**：四个 App 的 uniform 不同：
> - Direction：`light.direction`（vec4）、`viewPos`、`light.ambient/diffuse/specular`、`material.*`
> - Point：`light.position`、`light.constant/linear/quadratic`、`light.ambient/diffuse/specular`
> - Spot：`light.position/direction`、`light.cutOff/outerCutOff`、`light.ambient/diffuse/specular`
> - Mult：可能含多个光源（Direction/Point/Spot 组合），uniform 多且带数组。
> 逐一照原 .cpp 翻译为 `setUniform`（vec4→4 参 value_ptr，float→float 重载，int 采样器/shininess→int 重载）。Mult 的数组 uniform 需按原实现逐元素 `setUniform`（RHI 无数组便捷接口）。删除 GLProgram/glad/GLImageTexture2D include 与析构 glDelete*。

- [ ] **Step 4: 构建 + 运行验证**

Run: `./scripts/build_run.sh build`
Expected: 编译通过。

Run: `./scripts/run.sh all -b gl -d 1`
Expected: 46/46 OK。

Run: 逐个运行 Direction/Point/Spot/Mult（`./scripts/run.sh -a <AppName> -b gl -d 3`）
Expected: 渲染正常，旋转光源 + 多实例 Cube 网格，材质/光源交互正常。

- [ ] **Step 5: 提交**

```bash
git add src/app/GL/Light/LightSource/GLLightSourceDirection.* src/app/GL/Light/LightSource/GLLightSourcePoint.* src/app/GL/Light/LightSource/GLLightSourceSpot.* src/app/GL/Light/LightSource/GLLightSourceMult.*
git commit -m "refactor(app): migrate LightSource (Direction/Point/Spot/Mult) to RHI"
```

---

## 自审记录

- **Spec 覆盖**：T1→RhiGeometry 自定义 location（spec §2）；T2→无纹理 4 个（spec §3.1）；T3→Map（spec §3.2）；T4→LightSource 4 个（spec §3.2）。全部覆盖。
- **占位符扫描**：T2/T3/T4 存在「其余 uniform 照抄原 .cpp」的指引——这是有意为之（各 App uniform 集不同，无法逐一硬编码），已给出统一翻译规则（vec4→4 参 value_ptr、float→float、int→int）并要求 implementer 先读原 .cpp。T2 Step 2 示例含一段错误写法后立即用 Step 3 修正段覆盖，避免 implementer 误用。
- **类型一致性**：`RhiGeometry::Layout{uvLocation, normalLocation}` 在 T1 定义、T2/T3/T4 使用一致；`setUniform` 4 参重载 `(name, ptr, count, vecSize)` 与 `IPipeline.hpp:20` 一致。
- **需 implementer 核实的 API**：`renderer()->setIndexBuffer`、`renderer()->getDummyLayout`（T2 Step 3 提供 fallback）、`material.shininess` 的 int/float 类型（T3 Step 3 注明）。
