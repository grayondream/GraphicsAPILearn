#include "VKShader.hpp"
#include "base/Log.hpp"
#include <fstream>
#include <iterator>
#include <filesystem>

#ifndef RESOURCE_DIR
#define RESOURCE_DIR "res"
#endif

namespace rhi {

static std::string ToSpvSuffix(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return ".vert.spv";
        case ShaderStage::Fragment: return ".frag.spv";
        case ShaderStage::Geometry: return ".geom.spv";
        case ShaderStage::Compute:  return ".comp.spv";
    }
    return ".spv";
}

static std::string LocateSpv(const std::string& glslSource, ShaderStage::Type type) {
    namespace fs = std::filesystem;
    std::string suffix = ToSpvSuffix(type);
    // 直接给 .spv：原样返回
    if (fs::exists(glslSource) && glslSource.size() > 4 &&
        glslSource.compare(glslSource.size() - 4, 4, ".spv") == 0)
        return glslSource;
    // App 统一传 GLSL 源码路径（res/Vulkan/<dir>/<name>.{vert,frag,...}），
    // .spv 由 vk_shaders 目标生成于 build/res/Vulkan/<dir>/<name>.<stage>.spv。
    // 通过 RESOURCE_DIR 推断 build 产物路径。
    fs::path src(glslSource);
    fs::path srcNoExt = src;
    srcNoExt.replace_extension();
    fs::path resRoot(RESOURCE_DIR);
    resRoot = resRoot.lexically_normal();
    if (resRoot.is_relative())
        resRoot = fs::absolute(resRoot);
    fs::path buildVk = resRoot.parent_path() / "build" / "res" / "Vulkan";
    fs::path candidate = buildVk / srcNoExt.lexically_relative(fs::path(RESOURCE_DIR) / "Vulkan");
    candidate += suffix;
    if (fs::exists(candidate))
        return candidate.string();
    // 兜底：与源码同目录追加 .spv（cwd 为 build 或 res 复制树时）
    candidate = srcNoExt;
    candidate += suffix;
    if (fs::exists(candidate))
        return candidate.string();
    return glslSource;
}

static vk::ShaderStageFlagBits ToVkStage(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return vk::ShaderStageFlagBits::eVertex;
        case ShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
        case ShaderStage::Geometry: return vk::ShaderStageFlagBits::eGeometry;
        case ShaderStage::Compute:  return vk::ShaderStageFlagBits::eCompute;
    }
    return vk::ShaderStageFlagBits::eVertex;
}

bool VKShader::compile(const std::vector<ShaderStage>& stages) {
    _modules.clear();
    _log.clear();
    for (const auto& st : stages) {
        std::string spvPath = st.sourceIsSPIRV ? st.source : LocateSpv(st.source, st.type);
        std::ifstream f(spvPath, std::ios::binary);
        if (!f) {
            _log = "VKShader: cannot open " + spvPath;
            LOGE("{}", _log);
            return false;
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() % 4 != 0) {
            _log = "VKShader: invalid SPIR-V size for " + spvPath;
            LOGE("{}", _log);
            return false;
        }
        vk::ShaderModuleCreateInfo smci{};
        smci.codeSize = bytes.size();
        smci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
        auto mr = _dev.createShaderModule(smci);
        if (mr.result != vk::Result::eSuccess) {
            _log = "VKShader: createShaderModule failed for " + spvPath;
            LOGE("{}", _log);
            return false;
        }
        _modules.emplace(st.type, std::move(mr.value));
    }
    return !_modules.empty();
}

vk::ShaderModule VKShader::moduleFor(ShaderStage::Type type) const {
    auto it = _modules.find(type);
    return it != _modules.end() ? *it->second : vk::ShaderModule{};
}

std::vector<vk::PipelineShaderStageCreateInfo> VKShader::stageInfos() const {
    std::vector<vk::PipelineShaderStageCreateInfo> infos;
    infos.reserve(_modules.size());
    for (const auto& [type, module] : _modules) {
        vk::PipelineShaderStageCreateInfo info{};
        info.stage = ToVkStage(type);
        info.module = *module;
        info.pName = "main";
        infos.push_back(info);
    }
    return infos;
}

} // namespace rhi