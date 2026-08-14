#pragma once
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>

namespace rhi {

struct ULight {
    glm::vec4 position;   // position(xyz) + 可选类型标志(w)
    glm::vec4 direction;  // direction(xyz) + spot outerCutOff(w)
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 params;     // constant/linear/quadratic/cutOff(spot)
};

struct UniformBlock {
    glm::mat4  projection;
    glm::mat4  view;
    glm::mat4  model;
    glm::mat4  normalMatrix;
    glm::mat4  viewModel;
    glm::mat4  extraMat4[14];
    glm::vec4  vec4Pool[64];
    glm::vec4  vec3Pool[64];
    float      floatPool[64];
    ULight     lights[256];
};

static_assert(sizeof(UniformBlock) < 128 * 1024, "UBO exceeds Vulkan 128KB limit");

enum class SlotKind : uint8_t { Mat4, Mat3, Vec4, Vec3, Vec2, Float1, Int1, ULight_Field };

struct SlotInfo {
    const char* name;          // 槽名字符串（含 "material.diffuse" 等点语法；不含 "[N]" 后缀）
    // 目标是片段内绝对字节偏移（相对 UniformBlock 起始）
    std::size_t offset;
    // 对整数组 name（无 []）时,元素起点即 offset, elementBytes=stride
    std::size_t elementBytes;
    SlotKind kind;
};

const SlotInfo* FindSlot(const char* name);                       // nullptr if unknown

void SetUniform(UniformBlock& ubo, const char* name, const glm::mat4& v);
void SetUniform(UniformBlock& ubo, const char* name, const glm::mat3& v);
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec4& v);
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec3& v);
void SetUniform(UniformBlock& ubo, const char* name, const glm::vec2& v);
void SetUniform(UniformBlock& ubo, const char* name, float v);
void SetUniform(UniformBlock& ubo, const char* name, int v);
void SetUniform(UniformBlock& ubo, const char* name, bool v);
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::vec3& v);  // 数组/元素
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::vec4& v);
void SetUniform(UniformBlock& ubo, const char* name, uint32_t index, const glm::mat4& v);
void SetLight(UniformBlock& ubo, uint32_t index, const char* field, const glm::vec3& v);
void SetLightParam(UniformBlock& ubo, uint32_t index, const char* field, float v);

} // namespace rhi