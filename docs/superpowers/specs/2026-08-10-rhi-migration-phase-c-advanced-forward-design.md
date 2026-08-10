# 设计文档：子项目 B 计划C — BlinnPhong/Gamma/NormalMap/ParallaxMap 4 App 迁移到 RHI

日期：2026-08-10
状态：待评审

## 1. 背景与目标

子项目 B 主体为「46 App 全量迁移到 RHI」。已完成：前置补全、计划A（RHI 能力）、计划B（SimpleLight 9 App 迁移，确立 RhiGeometry + 双管线模板）。

本计划（计划C）迁移 Light/Advanced 下 4 个 forward App：**BlinnPhong、Gamma、NormalMap、ParallaxMap**。验证 RhiGeometry 模板在 advanced forward 场景的可复用性，并扩展 RhiGeometry 支持手写交错顶点数组。

### 范围：4 个 App

| App | 物体 | 光源 | 纹理 | 顶点形式 | 绘制 |
|---|---|---|---|---|---|
| BlinnPhong | Plane | Cube | wood.png | Shape（Plane/Cube） | 物体 drawArrays 6 + 光源 drawIndexed |
| Gamma | Plane | Cube | wood.png | Shape（Plane/Cube） | 物体 drawArrays 6 + 光源 drawIndexed |
| NormalMap | 手写 quad | 无独立光源形状 | brick/normal | 手写交错数组（14 float/顶点） | drawArrays 6 |
| ParallaxMap | 手写 quad | 无独立光源形状 | brick/normal/disp | 手写交错数组（14 float/顶点） | drawArrays 6 |

全部继承 `GLCameraBaseApp`（已 RHI 化）。所有 shader 为 `.vs/.fs` 扩展名。

## 2. 关键架构变更

### 2.1 RhiGeometry 新增「手写交错数组」入口（`CreateFromArray`）

NormalMap/ParallaxMap 用**手工硬编码交错顶点数组**（pos3 + normal3 + uv2 + tangent3 + bitangent3 = 14 float/顶点，单 VBO，`glDrawArrays(0,6)`，无 index），不是 Shape 类。现有 `RhiGeometry::Create` 只吃 `Shape&`，无法处理。

**方案（已确认）**：新增 `RhiGeometry::CreateFromArray(renderer, vertices, byteSize, vertexCount, layout)` 入口：
- 输入：原始交错顶点数据指针、总字节数、顶点数、目标 `VertexLayout`（调用方构造好各属性 offset/semantic/binding/stride）。
- 行为：创建单 `IBuffer`（`BufferType::Vertex`）上传数据，返回含 `vertexBuffer` + `layout` + `vertexCount` 的 `Geometry`。
- 不建 index buffer（手写 quad 用 drawArrays）。可选：后续 App 如需 index 可扩展，本计划 YAGNI 不做。
- 纯新增函数，不破坏现有 `Create`。

### 2.2 BlinnPhong/Gamma 复用现有 `RhiGeometry::Create`（默认布局）

BlinnPhong/Gamma 的 shader `Object.vs` 用 `pos=0, color=1, uv=2, normal=3`——与 RhiGeometry **默认布局一致**，无需自定义 location：
- 物体 = `Plane`：`RhiGeometry::Create(renderer, plane, true, true, false)`（useUv+useNormal，**useIndex=false** → 用 `vertexCount` + `renderer()->draw(6,0)`）。
- 光源 = `Cube`：`RhiGeometry::Create(renderer, cube, false, false, true)`（pos+color，indexed → `drawIndexed`）。
- 需确认 `useIndex=false` 时 `Geometry.vertexCount` 仍返回（现有实现如此）。

## 3. 各 App 迁移要点

### 3.1 BlinnPhong / Gamma（结构同）

- **物体管线**（Object.vs/fs）：`RhiGeometry::Create(renderer().get(), plane, true, true, false)` → `_planeVb/_uv/_normal/_vertexCount`（6），`createPipeline(geo.layout, shader)`，`setDepthTest(true)`。绘制：`setPipeline` + `setVertexBuffer(_vb)` + `setVertexBuffer(_uv,1)` + `setVertexBuffer(_normal,2)` + `draw(6, 0)`。
- **光源管线**（Source.vs/fs，仅 pos+color）：`RhiGeometry::Create(renderer().get(), cube, false, false, true)` 取 layout（复用 `_vb`/`_ebo`/`_indexCount`）。绘制：`drawIndexed`。
- **纹理**：`wood.png` → `RhiImage::Load2D` → `bindTexture(0)` + `setUniform("textureSampler", 0)`。
- **uniform 集**（逐 App 对照原 .cpp 保留）：
  - BlinnPhong：`projection/view/model/textureSampler/lightColor/lightPos(viewPos)/enableBlinnPhong`（bool）+ 光源 `projection/view/model/lightColor`。
  - Gamma：`projection/view/model/textureSampler/lightColor/lightPositions/lightColors(数组)/viewPos/enableGamma(gammaValue)`（bool/float）+ 光源块。Gamma 的 `lightPositions`/`lightColors` 是数组，RHI 无数组便捷接口，需逐元素 `setUniform(name+"[i]", ...)` 或按原实现方式翻译。
- **bool uniform**：`enableBlinnPhong`/`enableGamma` → `setUniform(name, bool)`（IRenderer.hpp:16 有 bool 重载）。
- 删除 GLProgram/glad/GLImageTexture2D include；析构删 glDelete*；保留光源+物体块。

### 3.2 NormalMap / ParallaxMap

- 顶点：`CreateRectBuffer()` 的手写交错数组 → `RhiGeometry::CreateFromArray(renderer, vertices, sizeof(vertices), 6, layout)`。layout 构造：
  - 单 VBO（binding 0）交错，stride = 14*sizeof(float)。
  - element 0: Float3, semantic=0, offset=0（pos）
  - element 1: Float3, semantic=1, offset=12（normal）
  - element 2: Float2, semantic=2, offset=24（uv）
  - element 3: Float3, semantic=3, offset=32（tangent）
  - element 4: Float3, semantic=4, offset=44（bitangent）
- 物体管线（NormalMap.vs/fs 或 ParallaxMap.vs/fs）：`createPipeline(geo.layout, shader)`，`setDepthTest(true)`。绘制：`setPipeline` + `setVertexBuffer(_vb)` + `draw(6, 0)`。
- 纹理：
  - NormalMap：`brickwall.jpg` → diffuse、`brickwall_normal.jpg` → normal map。
  - ParallaxMap：`brickwall.jpg` → diffuse、`brickwall_normal.jpg` → normal、`brickwall_height.jpg` → disp。
- uniform：`diffuseMap`(0)/`normalMap`(1)[/`depthMap`(2)] 采样器 int；`projection/view/model/viewPos/lightPos/enableNM(enableParallax)/heightScale(Parallax)` 等，逐 App 对照原 .cpp 保留。
- 删除 GLProgram/glad/GLImageTexture2D include；析构删 glDelete*。

## 4. 任务拆分（3 任务，SDD 执行）

| 任务 | 内容 | 文件 |
|---|---|---|
| **T1** | RhiGeometry 新增 `CreateFromArray`（手写交错数组入口） | `RhiGeometry.hpp/.cpp` |
| **T2** | BlinnPhong + Gamma 迁移（Plane 物体 drawArrays + Cube 光源 drawIndexed） | 2 App 的 .hpp/.cpp |
| **T3** | NormalMap + ParallaxMap 迁移（用 CreateFromArray） | 2 App 的 .hpp/.cpp |

每任务红线：`./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1` 46/46 OK；不重命名类名；只改对应文件。

## 5. 非目标（明确推迟）

- Hdr/Bloom/Defer（FBO/全屏 quad）——后续批次。
- Shadow×3（深度 cubemap RT/仅清深度，RHI 缺口）——后续批次，随补 RHI 能力。
- SSAO（多 pass + RGBA32F noise）——后续批次。
- `ToGLPrimitive` 显式 case、shininess float 化等计划A/B 技术债——留到 Vulkan 前统一。
- 去 GL 前缀重命名——收尾计划。

## 6. 验证

- 每任务：build OK + `run.sh all -b gl -d 1` 46/46 + 目标 App 单独运行视觉比对。
- T1 额外回归 Cube/Camera/LoadModel/SimpleLight（确认现有 Create 未破坏）。
- T2-T4 额外：BlinnPhong 开关（Enable Blinn Phong）、Gamma 开关（Enable Gamma + gammaValue 滑条）、NormalMap/ParallaxMap 法线贴图/高度贴图效果正常。

## 7. 参考文件

- 模板：`src/app/GL/Base/GLCameraApp.cpp`、`src/app/GL/Light/GLSimpleLightMap.cpp`（RHI 化样例）
- RhiGeometry：`src/app/GL/RhiGeometry.hpp/.cpp`
- 待迁：`src/app/GL/Light/Advanced/GLBlinnPhongApp.*`、`GLGammaApp.*`、`GLNormalMapApp.*`、`GLParallaxMapApp.*`
- 接口：`src/rhi/core/IRenderer.hpp`、`IPipeline.hpp`、`Common.hpp`
