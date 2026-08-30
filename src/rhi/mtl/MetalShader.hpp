#pragma once

#include "rhi/core/IShader.hpp"
#include <cstdint>
#include <unordered_map>

#if defined(__APPLE__)

#import <Metal/Metal.h>

namespace rhi::mtl {

class MetalShader : public IShader {
public:
    explicit MetalShader(void* device);
    ~MetalShader() override;

    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override;
    bool valid() const override;

    id<MTLLibrary> library() const { return _library; }
    id<MTLFunction> vertexFunction(const std::string& name);
    id<MTLFunction> fragmentFunction(const std::string& name);
    id<MTLFunction> vertexFunction();
    id<MTLFunction> fragmentFunction();

private:
    void* _device{nullptr};
    id<MTLLibrary> __strong _library{nil};
    std::string _log;
    std::string _vertexEntry;
    std::string _fragmentEntry;
    std::unordered_map<std::string, __strong id<MTLFunction>> _functionCache;
};

} // namespace rhi::mtl

#else // non-Apple fallback

namespace rhi::mtl {

class MetalShader : public IShader {
public:
    MetalShader() = default;
    ~MetalShader() override = default;

    bool compile(const std::vector<ShaderStage>&) override { return false; }
    std::string getLog() const override { return "Metal not supported on this platform"; }
    bool valid() const override { return false; }

    void* vertexFunction(const std::string&) { return nullptr; }
    void* fragmentFunction(const std::string&) { return nullptr; }
    void* vertexFunction() { return nullptr; }
    void* fragmentFunction() { return nullptr; }
};

} // namespace rhi::mtl

#endif
