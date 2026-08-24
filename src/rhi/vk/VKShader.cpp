#include "VKShader.hpp"
#include "base/AppDirs.hpp"
#include "base/Log.hpp"
#include <fstream>
#include <iterator>
#include <filesystem>

#ifndef RESOURCE_DIR
#define RESOURCE_DIR "res"
#endif

namespace rhi {

// 多配置构建目录：从 exe 自身位置向上找含 res/<Backend> 产物树的目录
// （exe 位于 <cfg>/src[/Release]/，资源树在 <cfg>/res/），任意命名的
// 配置目录（build/build-nodx/build-dev…）均可无缝运行。
static std::vector<std::filesystem::path> ArtifactRoots(const char* backendDir) {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    fs::path dir = fs::path(Utils::AppDirs::ExePath()).parent_path();
    for (int i = 0; i < 6 && !dir.empty(); ++i) {
        if (fs::is_directory(dir / "res" / backendDir))
            roots.push_back(dir / "res" / backendDir);
        dir = dir.parent_path();
    }
    return roots;
}

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
    const fs::path rel = srcNoExt.lexically_relative(fs::path(RESOURCE_DIR) / "Vulkan");
    for (const auto& root : ArtifactRoots("Vulkan")) {
        fs::path candidate = root / rel;
        candidate += suffix;
        if (fs::exists(candidate))
            return candidate.string();
    }
    // 兼容旧布局：仓库根固定 build/ 目录
    fs::path legacyRoot = resRoot.parent_path() / "build" / "res" / "Vulkan";
    fs::path candidate = legacyRoot / rel;
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