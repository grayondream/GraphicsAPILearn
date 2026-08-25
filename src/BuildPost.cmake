# 资源解析约定（无 POST_BUILD 拷贝）：
#   - 纹理/模型等运行时资源：RESOURCE_DIR（configure 期生成的源树 res 绝对路径）
#   - 着色器产物：vk_shaders/dx_shaders 目标写入 ${CMAKE_BINARY_DIR}/res/<Backend>，
#     运行期经 DXShader/VKShader 的 ArtifactRoots（exe 相对向上探测 res/<Backend>）解析
#   - 旧版曾把整个 res 拷贝到 exe 旁 "Res"（大写，与 ArtifactRoots 的小写 res 不匹配，
#     Linux 下为死拷贝；Windows 大小写不敏感也从不被读取）——已移除，节省全量拷贝耗时
