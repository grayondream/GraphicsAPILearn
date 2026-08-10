# 设计文档：子项目 B 计划B — SimpleLight 9 App 迁移到 RHI

日期：2026-08-09
状态：待评审

## 1. 背景与目标

子项目 B 主体为「46 App 全量迁移到 RHI」。已完成：前置补全（顶点装配、RhiGeometry/RhiImage、5 基础 App + LoadModel 模板）、计划A（RHI 能力补全：PrimitiveType/mat3 矩阵 uniform/RGBA32F/HDR 加载/Int4 整数指针/GLCameraBaseApp RHI 化）。

本计划（计划B）为第一批 App 迁移：**Light 目录下的 SimpleLight 9 个 App**，验证 RhiGeometry 模板的可复制性。

### 范围：9 个 App

| 类别 | App | 纹理 | 顶点 |
|---|---|---|---|
| Light/ 无纹理 | Ambination、Diffuse、Specular、Material | 无 | Sphere |
| Light/ 有纹理 | Map | diffuse+specular 双纹理 | Sphere |
| LightSource/ 有纹理 | Direction、Point、Spot、Mult | diffuse+specular 双纹理 | Cube |

全部继承 `GLCameraBaseApp`（已 RHI 化），结构一致：光源 shader + 物体 shader、Sphere/Cube 顶点、`glEnable(GL_DEPTH_TEST)`、无 FBO。

## 2. 关键架构变更：RhiGeometry 自定义 location

### 2.1 冲突

SimpleLight 的 shader 顶点布局与已迁移模板 RhiGeometry 不兼容：

- **RhiGeometry 模板**（Cube/Camera/LoadModel）固定：`pos=0, inColor=1, uv=2, normal=3`（normal 恒在 semantic=3）
- **SimpleLight shader**（Object.vert）固定：`pos=0, inColor=1, aNormal=2, uv=3`（normal=2，LightMap/LightSource 的 Object.vert 额外用 uv=3）

即 normal 与 uv 的 location 互换。

### 2.2 方案（已确认）

**扩展 `RhiGeometry::Create` 支持自定义 normal/uv 的 location（semantic）**，默认保持现布局（uv=2, normal=3）以不破坏已迁移 App，通过可选参数实现向后兼容。

- 新增可选配置结构 `RhiGeometry::Layout`（字段 `uvLocation`、`normalLocation`，默认 2/3）。
- `Create(renderer, shape, useUv, useNormal, useIndex, const Layout& = {})` 末尾追加参数。
- SimpleLight 调用时传 `{.uvLocation=3, .normalLocation=2}`。
- 仅影响 `layout.elements[]` 中 uv/normal 元素的 `semantic` 值；binding 槽不变（uv=binding1、normal=binding2）。GL 端 semantic=location，与 buffer binding 独立，改动安全。
- pos/color 恒为 semantic 0/1，不变。

## 3. 各 App 迁移要点

### 3.1 无纹理 5 个（Ambination/Diffuse/Specular/Material）

- 光源 shader（Light.vert/frag）→ 创建第二个 `IPipeline`（仅 pos+color，无 normal/uv）。
- 物体 shader（Object.vert/frag）→ 创建主 `IPipeline`（pos+color+normal，normal=2）。
- `createVertexBuffer()` 的 raw VAO/VBO/EBO（glGen*/glDelete/glVertexAttribPointer）→ 用 `RhiGeometry::Create(renderer, shape, useUv=false, useNormal=true, useIndex=true, {.normalLocation=2})` 生成。
- 顶点数据源：`shape.toGL().data()`（pos+color 交错）、`shape.normal()`。数据格式与 RhiGeometry 已支持一致。
- 移除析构中的 `glDelete*`，改由 `shared_ptr<IBuffer>`/`IPipeline` RAII。
- `glBindVertexArray`/`glBindVertexArray(0)` → `renderer()->setPipeline()` + `setVertexBuffer`。
- `glEnable(GL_DEPTH_TEST)` → `pipeline->setDepthTest(true)`（物体管线）。
- 删除 `native/GL/GLProgram.hpp`、`glad/glad.h`、`native/GL/GLImageTexture2D.hpp` include；头文件删除 `GLProgram` 成员，换 `std::shared_ptr<rhi::IPipeline>`。

### 3.2 有纹理 5 个（Map + LightSource Direction/Point/Spot/Mult）

- 在 3.1 基础上，物体管线额外启用 uv（`useUv=true`），`RhiGeometry::Create(..., {.uvLocation=3, .normalLocation=2})`。
- `GLImageTexture2D`（`_objTex`/`_objBorderTex`）→ `RhiImage::Load2D`（`shared_ptr<ITexture2D>`）。
- `texture()->bind(n)` → `renderer()->bindTexture(tex, n)`。
- `material.diffuse/specular`（纹理采样器 uniform）→ `pipeline->setUniform(name, int sampleUnit)`。
- `material.shininess`（标量 int）→ `pipeline->setUniform(name, int)`。
- 注意 LightSource 的 object 循环使用**同一个物体管线 + 每实例 setUniform("model")** + `renderer()->draw`，验证 draw 路径多实例复用。
- LightSource 物体为 Cube（`geometry/Cube.hpp`），Light/ 为 Sphere；两者数据格式同构，共用 RhiGeometry。

### 3.3 uniform 迁移映射

| GLProgram 调用 | RHI 等价 |
|---|---|
| `update("projection", projection)` | `setUniform("projection", glm::value_ptr(p), 1)` |
| `update("view", view)` / `update("model", model)` | 同上（mat4，`vecSize` 默认 4，走 3 参重载） |
| `update("lightColor", glm::vec4)` | `setUniform("lightColor", glm::value_ptr(&v), 1, 4)` |
| `update("material.shininess", 1)` | `setUniform("material.shininess", 1)`（int 重载） |
| `update("material.diffuse", 0)` | `setUniform("material.diffuse", 0)`（int 重载，采样器） |

参考已迁移 GLCameraApp 用 3 参 `setUniform(name, ptr, count)` 传 mat4；vec4 用 4 参（`vecSize=4`）。

## 4. 任务拆分（4 任务，SDD 执行）

| 任务 | 内容 | 文件 |
|---|---|---|
| **T1** | RhiGeometry::Create 支持自定义 location（新增 `Layout` 可选参数，默认 uv=2/normal=3） | `RhiGeometry.hpp/.cpp` |
| **T2** | 无纹理 SimpleLight 迁移（Ambination/Diffuse/Specular/Material） | 4 App 的 .hpp/.cpp |
| **T3** | SimpleLightMap 迁移（有纹理，含双纹理+shininess） | GLSimpleLightMap .hpp/.cpp |
| **T4** | LightSource 4 个迁移（Direction/Point/Spot/Mult，Cube+双纹理+多实例） | 4 App 的 .hpp/.cpp |

每任务红线：`./scripts/build_run.sh build` + `./scripts/run.sh all -b gl -d 1` 46/46 OK；不重命名类名；只改对应文件。

## 5. 非目标（明确推迟）

- PBR 批次技术债（4 个 PBR 头补 `GLImageTexture2D` 声明并删 GLCameraBaseApp 前置声明）——留到 PBR 批次。
- `ToGLPrimitive` 显式 case（P1）、`setUniformMatrix` else（P2）等——留到后续。
- 深度 cubemap RT / attachCubeFace mip / 仅清深度——留到 Light/PBR 批次。
- 去 GL 前缀重命名——留到收尾计划。

## 6. 验证

- 每任务：build OK + `run.sh all -b gl -d 1` 46/46 + 目标 App 单独运行视觉比对。
- T1 额外回归 Cube/Camera/LoadModel（确认默认布局未变）。
- T2-T4 额外：无纹理与有纹理渲染结果与原实现一致（光源位置、颜色拾取器、材质参数交互正常）。

## 7. 参考文件

- 模板：`src/app/GL/Base/GLCameraApp.cpp`、`GLCubeApp.cpp`（RHI 化样例）
- RhiGeometry：`src/app/GL/RhiGeometry.hpp/.cpp`
- 待迁：`src/app/GL/Light/GLSimpleLight*.{hpp,cpp}`、`src/app/GL/Light/LightSource/GLLightSource*.{hpp,cpp}`
- 接口：`src/rhi/core/IPipeline.hpp`、`src/rhi/core/Common.hpp`
