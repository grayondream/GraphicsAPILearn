#include "rhi/core/UniformBlock.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace rhi {

using UBO = UniformBlock;
// constexpr 偏移基准（避免 magic number）
constexpr std::size_t V4(std::size_t i) { return offsetof(UBO, vec4Pool) + 16 * i; }
constexpr std::size_t V3(std::size_t i) { return offsetof(UBO, vec3Pool) + 16 * i; }
constexpr std::size_t F4(std::size_t i) { return offsetof(UBO, floatPool) + 4 * i; }
constexpr std::size_t LGT(std::size_t i) { return offsetof(UBO, lights) + sizeof(ULight) * i; }
constexpr std::size_t XM4(std::size_t i) { return offsetof(UBO, extraMat4) + 64 * i; }

// —— 官方池索引表（App 字符串 → 池内索引）——
// vec4Pool（每个 App 写入它需要的槽；shader 读取同索引）:
//   0=viewPos  1=cameraPos(camPos 同)  2=lightPos  3=lightColor  4=objectColor
//   5=cubeColor  6=texColor  7=material.ambient  8=material.diffuse(颜色变体)  9=material.specular
//   10..12=material 自由预留  13..28=lightPositions[i]    29..44=lightColors[i]    45=radiusPos(Saturn) 46..63=自由
// vec3Pool:  0..63 = SSAO samples[64]（唯一消费者）
// floatPool:
//   0=shininess  1=ambientStrength  2=specularStrength  3=diffuseStrength
//   4=exposure  5=gammaValue  6=heightScale  7=metallic  8=roughness  9=ao  10=time
//   11=bloom  12=horizontal  13=hdr  14=count  15=type  16=near_plane  17=far_plane
//   18=debug  19=shadows  20=enableGamma  21=enableBlinnPhong  22=enableNM  23=enableDisp
//   24=enableOcclusion  25=enableBias ... (enable* 依次 20..33)
//   34/35=inverse_normals/reverse_normals·invertedNormals  36=enableReflection  37=enableRefraction
//   38=enableFragCoord  39..42 = enableSSAO/enableSimplePCF/enableSteep/enableVertexId
//   43/44=enablePointSize/enableFrontFaceCulling  45=light(点光阴影) 46..63=自由
// extraMat4: 0=lightSpaceMatrix（单灯）  1..6=shadowMatrices[0..5]（点光 6 面）  7..13=自由
// lights[i]: 0=dirLight  1..4=pointLights  5=spotLight  6..255=Defer/Hdr/Bloom lights[i]

namespace {

// 静态槽表（FindSlot 线性扫）。offset 为相对 UniformBlock 的绝对偏移。
static const SlotInfo kSlots[] = {
    { "projection",        offsetof(UBO, projection),       64, SlotKind::Mat4 },
    { "view",              offsetof(UBO, view),             64, SlotKind::Mat4 },
    { "model",             offsetof(UBO, model),            64, SlotKind::Mat4 },
    { "viewModel",         offsetof(UBO, viewModel),        64, SlotKind::Mat4 },
    { "lightSpaceMatrix",  XM4(0), 64, SlotKind::Mat4 },
    { "shadowMatrices",    XM4(1), 64, SlotKind::Mat4 },   // 数组 [0..5] → extraMat4[1+i]
    { "normalMatrix",      offsetof(UBO, normalMatrix),     64, SlotKind::Mat3 }, // 容器为 mat4,写入时转
    { "viewPos",           V4(0), 16, SlotKind::Vec4 },
    { "cameraPos",         V4(1), 16, SlotKind::Vec4 },
    { "camPos",            V4(1), 16, SlotKind::Vec4 },
    { "lightPos",          V4(2), 16, SlotKind::Vec4 },
    { "lightColor",        V4(3), 16, SlotKind::Vec4 },
    { "objectColor",       V4(4), 16, SlotKind::Vec4 },
    { "cubeColor",         V4(5), 16, SlotKind::Vec4 },
    { "texColor",          V4(6), 16, SlotKind::Vec4 },
    { "material.ambient",  V4(7), 16, SlotKind::Vec4 },
    { "material.diffuse",  V4(8), 16, SlotKind::Vec4 },   // 颜色变体（采样器变体不进 block）
    { "material.specular", V4(9), 16, SlotKind::Vec4 },
    { "radiusPos",         V4(45), 16, SlotKind::Vec4 },  // Saturn 岩石环中心（vec3 写低 12 字节）
    { "lightPositions",    V4(13), 16, SlotKind::Vec4 },  // 数组 [0..15]
    { "lightColors",       V4(29), 16, SlotKind::Vec4 },  // 数组 [0..15]
    { "samples",           V3(0), 16, SlotKind::Vec3 },   // SSAO [0..63]
    { "shininess",         F4(0),  4, SlotKind::Float1 },
    { "material.shininess",F4(0),  4, SlotKind::Float1 },
    { "ambientStrength",   F4(1),  4, SlotKind::Float1 },
    { "specularStrength",  F4(2),  4, SlotKind::Float1 },
    { "diffuseStrength",   F4(3),  4, SlotKind::Float1 },
    { "exposure",          F4(4),  4, SlotKind::Float1 },
    { "gammaValue",        F4(5),  4, SlotKind::Float1 },
    { "heightScale",       F4(6),  4, SlotKind::Float1 },
    { "metallic",          F4(7),  4, SlotKind::Float1 },
    { "roughness",         F4(8),  4, SlotKind::Float1 },
    { "ao",                F4(9),  4, SlotKind::Float1 },
    { "time",              F4(10), 4, SlotKind::Float1 },
    { "times",             F4(10), 4, SlotKind::Float1 },
    { "bloom",             F4(11), 4, SlotKind::Float1 },
    { "horizontal",        F4(12), 4, SlotKind::Float1 },
    { "hdr",               F4(13), 4, SlotKind::Float1 },
    { "count",             F4(14), 4, SlotKind::Float1 },
    { "type",              F4(15), 4, SlotKind::Float1 },
    { "effectType",        F4(15), 4, SlotKind::Float1 },
    { "near_plane",        F4(16), 4, SlotKind::Float1 },
    { "far_plane",         F4(17), 4, SlotKind::Float1 },
    { "debug",             F4(18), 4, SlotKind::Float1 },
    { "shadows",           F4(19), 4, SlotKind::Float1 },
    { "enableGamma",       F4(20), 4, SlotKind::Float1 },
    { "enableBlinnPhong",  F4(21), 4, SlotKind::Float1 },
    { "enableNM",          F4(22), 4, SlotKind::Float1 },
    { "enableDisp",        F4(23), 4, SlotKind::Float1 },
    { "enableOcclusion",   F4(24), 4, SlotKind::Float1 },
    { "enableBias",        F4(25), 4, SlotKind::Float1 },
    { "inverse_normals",   F4(34), 4, SlotKind::Float1 },
    { "reverse_normals",   F4(34), 4, SlotKind::Float1 },   // 别名：点光阴影外部大盒法向反转
    { "invertedNormals",   F4(35), 4, SlotKind::Float1 },
    { "light",             F4(45), 4, SlotKind::Float1 },   // 点光阴影灯位（bool）落自由区
    { "enableReflection",  F4(36), 4, SlotKind::Float1 },
    { "enableRefraction",  F4(37), 4, SlotKind::Float1 },
    { "enableFragCoord",   F4(38), 4, SlotKind::Float1 },
    { "enableSSAO",        F4(39), 4, SlotKind::Float1 },
    { "enableSimplePCF",   F4(40), 4, SlotKind::Float1 },
    { "enableSteep",       F4(41), 4, SlotKind::Float1 },
    { "enableVertexId",    F4(42), 4, SlotKind::Float1 },
    { "enablePointSize",   F4(43), 4, SlotKind::Float1 },
    { "enableFrontFaceCulling", F4(44), 4, SlotKind::Float1 },
    { "enableVolume",      F4(46), 4, SlotKind::Float1 },   // Defer 体积光照裁剪（自由区）
    { "lights",            LGT(0), sizeof(ULight), SlotKind::ULight_Field }, // 数组
};

const SlotInfo* FindSlotView(std::string_view nv) {
    for (const auto& s : kSlots) {
        if (nv == std::string_view(s.name)) return &s;
    }
    return nullptr;
}

// 解析 "name[N]" 或 "name.member"：
// - 若命中 kSlots 且其后跟 '[' → 数组索引 N，offset += N*elementBytes
// - 若命中 kSlots(light 系 "light.Position" 等) → 交给 SetLight/SetUniform(light...)
// - 精确命中 → 单槽
// 未知名 → 静默返回（与 GL 便捷层 setUniform 查找返回 false 同语义，不崩）。
bool ParseIndex(const char* name, std::string_view& base, long& idx) {
    // "xxx[12]" → base="xxx", idx=12；无 '[' → idx=0
    if (!name) return false;
    const char* br = std::strchr(name, '[');
    if (br) {
        base = std::string_view(name, static_cast<std::size_t>(br - name));
        idx = std::strtol(br + 1, nullptr, 10);
        return true;
    }
    base = std::string_view(name);
    idx = 0;
    return false;
}

template <class T>
void WriteAt(UniformBlock& ubo, const char* name, long idx, const T& v, const void* p, size_t sz) {
    (void)v;  // 仅用于模板参数推导
    std::string_view base; long i = 0;
    ParseIndex(name, base, i);
    const SlotInfo* s = FindSlotView(base);  // base 为名字前缀（可含点语法，不含 "[N]"）
    if (!s) return;
    const std::size_t element = static_cast<std::size_t>(i) + static_cast<std::size_t>(idx);
#ifdef SLOT_ENABLE_CHECKS
    assert(s->offset + s->elementBytes <= sizeof(UniformBlock));
    assert(s->offset + element * s->elementBytes + sz <= sizeof(UniformBlock));
#endif
    char* dst = reinterpret_cast<char*>(&ubo) + s->offset + element * s->elementBytes;
    std::memcpy(dst, p, sz);
}

} // namespace

const SlotInfo* FindSlot(const char* name) {
    if (!name) return nullptr;
    return FindSlotView(std::string_view(name));
}

void SetUniform(UniformBlock& ubo, const char* name, const glm::mat4& v) { glm::mat4 m = v; WriteAt(ubo, name, 0, m, glm::value_ptr(m), 64); }
void SetUniform(UniformBlock& ubo, const char* name, const glm::mat3& v) {
    // 容器 normalMatrix 是 glm::mat4：先还原为 mat4 再整体写
    glm::mat4 m(1.f); for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) m[c][r] = v[c][r];
    WriteAt(ubo, name, 0, m, glm::value_ptr(m), 64);
}
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec4& v) { WriteAt(ubo, name, 0, v, glm::value_ptr(v), 16); }
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec3& v) { WriteAt(ubo, name, 0, v, glm::value_ptr(v), 12); }
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec2& v) { WriteAt(ubo, name, 0, v, glm::value_ptr(v), 8); }
void SetUniform(UniformBlock& ubo, const char* name, float v) { WriteAt(ubo, name, 0, v, &v, 4); }
void SetUniform(UniformBlock& ubo, const char* name, int v) { float f = (float)v; WriteAt(ubo, name, 0, f, &f, 4); }
void SetUniform(UniformBlock& ubo, const char* name, bool v) { float f = v ? 1.f : 0.f; WriteAt(ubo, name, 0, f, &f, 4); }
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::vec3& v) { WriteAt(ubo, name, static_cast<long>(index), v, glm::value_ptr(v), 12); }
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::vec4& v) { WriteAt(ubo, name, static_cast<long>(index), v, glm::value_ptr(v), 16); }
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::mat4& v) { glm::mat4 m = v; WriteAt(ubo, name, static_cast<long>(index), m, glm::value_ptr(m), 64); }

// 灯光（SetLight demux field→ULight 成员，SetLightParam 写 params 分量）
void SetLight(UniformBlock& ubo, uint32_t index, const char* field, const glm::vec3& v) {
    if (index >= 256) return;
    ULight& L = ubo.lights[index];
    if (!field) return;
    if (strcmp(field, "position") == 0)      { L.position = glm::vec4(v, 0.f); }
    else if (strcmp(field, "ambient") == 0)  { L.ambient = glm::vec4(v, 0.f); }
    else if (strcmp(field, "diffuse") == 0)  { L.diffuse = glm::vec4(v, 0.f); }
    else if (strcmp(field, "specular") == 0) { L.specular = glm::vec4(v, 0.f); }
    // "direction" 存 direction.xyz（w 留给 spot outerCutOff），不碰 position
    else if (strcmp(field, "direction") == 0){ L.direction.x = v.x; L.direction.y = v.y; L.direction.z = v.z; }
    else if (strcmp(field, "Position") == 0) { L.position = glm::vec4(v, 0.f); }
    else if (strcmp(field, "Color") == 0)    { L.diffuse = glm::vec4(v, 0.f); }
    // 参数类由 SetLightParam 写（constant→params.x, linear→params.y, quadratic→params.z,
    // cutOff→params.w, 若需求外切角 outerCutOff→复用 direction.w）
}
void SetLightParam(UniformBlock& ubo, uint32_t index, const char* field, float v) {
    if (index >= 256) return;
    ULight& L = ubo.lights[index];
    if (!field) return;
    if (strcmp(field, "constant") == 0)      L.params.x = v;
    else if (strcmp(field, "linear") == 0)   L.params.y = v;
    else if (strcmp(field, "quadratic") == 0)L.params.z = v;
    else if (strcmp(field, "cutOff") == 0)   L.params.w = v;
    else if (strcmp(field, "outerCutOff") == 0) L.direction.w = v;
    else if (strcmp(field, "Radius") == 0)   L.direction.w = v;  // Defer 光度体积半径（避免与 quadratic/params.z 冲突）
    else if (strcmp(field, "type") == 0)     L.position.w = v; // 0=点/聚光, 1=方向光
}

} // namespace rhi