#pragma once
#include "VKHeader.hpp"
#include "rhi/core/IShader.hpp"
#include <map>

namespace rhi {

class VKShader : public IShader {
public:
    explicit VKShader(vk::raii::Device& device) : _dev(device) {}

    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return !_modules.empty(); }

    bool hasStage(ShaderStage::Type type) const { return _modules.count(type) != 0; }
    vk::ShaderModule moduleFor(ShaderStage::Type type) const;
    std::vector<vk::PipelineShaderStageCreateInfo> stageInfos() const;

private:
    vk::raii::Device& _dev;
    std::map<ShaderStage::Type, vk::raii::ShaderModule> _modules{};
    std::string _log{};
};

} // namespace rhi