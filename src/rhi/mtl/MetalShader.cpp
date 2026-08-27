#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "MetalShader.hpp"
#include "utils/FileUtils.hpp"
#include "base/Log.hpp"

namespace rhi::mtl {

MetalShader::MetalShader(void* device)
    : _device((__bridge id<MTLDevice>)device) {}

MetalShader::~MetalShader() {
    for (auto& [name, func] : _functionCache) {
        if (func) {
            CFRelease((__bridge CFTypeRef)func);
            func = nil;
        }
    }
    _functionCache.clear();
    _library = nil;
    _device = nil;
}

bool MetalShader::compile(const std::vector<ShaderStage>& stages) {
    _log.clear();
    _library = nil;

    for (auto& [name, func] : _functionCache) {
        if (func) {
            CFRelease((__bridge CFTypeRef)func);
            func = nil;
        }
    }
    _functionCache.clear();

    if (!_device) {
        _log = "MetalShader: invalid device";
        LOGE("MetalShader: invalid device");
        return false;
    }

    for (const auto& stage : stages) {
        if (stage.type == ShaderStage::Geometry) {
            _log = "MetalShader: geometry shaders not supported in Metal";
            LOGE("MetalShader: geometry shaders not supported in Metal");
            return false;
        }
        if (stage.type == ShaderStage::Compute) {
            _log = "MetalShader: compute shaders not yet supported";
            LOGE("MetalShader: compute shaders not yet supported");
            return false;
        }
    }

    for (const auto& stage : stages) {
        if (stage.source.empty()) {
            _log = "MetalShader: empty source path";
            LOGE("MetalShader: empty source path");
            return false;
        }

        std::string source = FileUtils::readFile2String(stage.source);
        if (source.empty()) {
            _log = "MetalShader: failed to read source file: " + stage.source;
            LOGE("MetalShader: failed to read source file: {}", stage.source);
            return false;
        }

        NSString* mtlSource = [NSString stringWithUTF8String:source.c_str()];
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.languageVersion = MTLLanguageVersion3_0;

        NSError* error = nil;
        id<MTLLibrary> lib = [_device newLibraryWithSource:mtlSource
                                                  options:options
                                                    error:&error];
        if (!lib || error) {
            NSString* errDesc = error ? [error localizedDescription] : @"unknown error";
            _log = "MetalShader: compilation failed (" + stage.source + "): "
                 + [errDesc UTF8String];
            LOGE("MetalShader: compilation failed ({}): {}", stage.source, [errDesc UTF8String]);
            return false;
        }

        _library = lib;
        LOGI("MetalShader: compiled successfully: {}", stage.source);
        break;
    }

    return _library != nil;
}

std::string MetalShader::getLog() const {
    return _log;
}

bool MetalShader::valid() const {
    return _library != nil;
}

id<MTLFunction> MetalShader::vertexFunction(const std::string& name) {
    if (!_library || name.empty()) return nil;

    auto it = _functionCache.find(name);
    if (it != _functionCache.end()) return it->second;

    NSString* funcName = [NSString stringWithUTF8String:name.c_str()];
    id<MTLFunction> func = [_library newFunctionWithName:funcName];
    if (!func) {
        LOGE("MetalShader: vertex function '{}' not found", name);
        return nil;
    }

    id<MTLFunction> retained = (__bridge_retained id<MTLFunction>)(__bridge void*)func;
    _functionCache[name] = retained;
    return retained;
}

id<MTLFunction> MetalShader::fragmentFunction(const std::string& name) {
    return vertexFunction(name);
}

} // namespace rhi::mtl

#endif
