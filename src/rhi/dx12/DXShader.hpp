#pragma once
#include "rhi/dx12/DXHeader.hpp"
#include "rhi/core/IShader.hpp"
#include <map>

namespace rhi {

class DXShader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return !_blobs.empty(); }

    bool hasStage(ShaderStage::Type type) const { return _blobs.count(type) != 0; }
    // 返回独立引用（AddRef 拷贝），调用方经 ComPtr 释放
    ComPtr<ID3DBlob> moduleFor(ShaderStage::Type type) const;

private:
    std::map<ShaderStage::Type, ComPtr<ID3DBlob>> _blobs{};
    std::string _log{};
};

} // namespace rhi
