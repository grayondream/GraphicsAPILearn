#include "VKShader.hpp"
#include "base/Log.hpp"
#include <fstream>
#include <iterator>

namespace rhi {

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
        if (!st.sourceIsSPIRV) {
            _log = "VKShader: non-SPIR-V source provided for stage " + st.source;
            LOGE("{}", _log);
            return false;
        }
        std::ifstream f(st.source, std::ios::binary);
        if (!f) {
            _log = "VKShader: cannot open " + st.source;
            LOGE("{}", _log);
            return false;
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() % 4 != 0) {
            _log = "VKShader: invalid SPIR-V size for " + st.source;
            LOGE("{}", _log);
            return false;
        }
        vk::ShaderModuleCreateInfo smci{};
        smci.codeSize = bytes.size();
        smci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
        auto mr = _dev.createShaderModule(smci);
        if (mr.result != vk::Result::eSuccess) {
            _log = "VKShader: createShaderModule failed for " + st.source;
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