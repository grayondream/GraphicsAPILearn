#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "MetalShader.hpp"
#include "utils/FileUtils.hpp"
#include "base/Log.hpp"
#include <cctype>

namespace rhi::mtl {

namespace {
// 去除 MSL 源码中的 // 与 /* */ 注释，避免入口名扫描误命中注释中的关键字。
std::string stripMetalComments(std::string src) {
    std::string out;
    out.reserve(src.size());
    size_t i = 0, n = src.size();
    while (i < n) {
        if (src[i] == '/' && i + 1 < n && src[i + 1] == '/') {
            while (i < n && src[i] != '\n') ++i;
        } else if (src[i] == '/' && i + 1 < n && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            i += 2;
        } else {
            out += src[i];
            ++i;
        }
    }
    return out;
}

// 从 MSL 源码中扫描形如 `vertex <ret> Name(` / `fragment <ret> Name(` 的入口名。
std::string scanMetalEntry(const std::string& src, const std::string& keyword) {
    std::string key = keyword + " ";
    size_t pos = src.find(key);
    if (pos == std::string::npos) return "";
    size_t paren = src.find('(', pos);
    if (paren == std::string::npos || paren == 0) return "";
    size_t start = paren;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(src[start - 1])) || src[start - 1] == '_')) {
        --start;
    }
    return src.substr(start, paren - start);
}
} // namespace

MetalShader::MetalShader(void* device)
    : _device(device) {}

MetalShader::~MetalShader() {
    _functionCache.clear();
    _library = nil;
    _device = nullptr;
}

bool MetalShader::compile(const std::vector<ShaderStage>& stages) {
    _log.clear();
    _library = nil;
    _vertexEntry.clear();
    _fragmentEntry.clear();

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

        // Metal 采用单文件 .metal（顶点+片元同文件），统一将任意 GLSL 扩展名映射到 .metal。
        std::string realPath = stage.source;
        size_t dot = realPath.find_last_of('.');
        if (dot != std::string::npos) {
            realPath.replace(dot, std::string::npos, ".metal");
        }

        if (_library) break;

        std::string source = FileUtils::readFile2String(realPath);
        if (source.empty()) {
            _log = "MetalShader: failed to read source file: " + realPath;
            LOGE("MetalShader: failed to read source file: {}", realPath);
            return false;
        }

        // Metal 入口名与 GLSL 不同（如 triangle_vertex），从源码中扫描得到。
        std::string scanSrc = stripMetalComments(source);
        if (_vertexEntry.empty()) {
            auto re = scanMetalEntry(scanSrc, "vertex");
            if (!re.empty()) _vertexEntry = re;
        }
        if (_fragmentEntry.empty()) {
            auto fe = scanMetalEntry(scanSrc, "fragment");
            if (!fe.empty()) _fragmentEntry = fe;
        }

        NSString* mtlSource = [NSString stringWithUTF8String:source.c_str()];
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.languageVersion = MTLLanguageVersion3_0;

        id<MTLDevice> device = (__bridge id<MTLDevice>)_device;
        NSError* error = nil;
        id<MTLLibrary> lib = [device newLibraryWithSource:mtlSource
                                                    options:options
                                                      error:&error];
        if (!lib || error) {
            NSString* errDesc = error ? [error localizedDescription] : @"unknown error";
            _log = "MetalShader: compilation failed (" + realPath + "): "
                 + [errDesc UTF8String];
            LOGE("MetalShader: compilation failed ({}): {}", realPath, [errDesc UTF8String]);
            return false;
        }

        _library = lib;
        LOGI("MetalShader: compiled successfully: {}", realPath);
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
        NSMutableString* dbg = [NSMutableString stringWithString:@"available="];
        for (NSString* fn in [_library functionNames]) { [dbg appendFormat:@" %@", fn]; }
        LOGE("MetalShader: vertex function '{}' not found. {}", name, [dbg UTF8String]);
        return nil;
    }

    _functionCache[name] = func;
    return func;
}

id<MTLFunction> MetalShader::fragmentFunction(const std::string& name) {
    return vertexFunction(name);
}

id<MTLFunction> MetalShader::vertexFunction() {
    return vertexFunction(_vertexEntry);
}

id<MTLFunction> MetalShader::fragmentFunction() {
    return fragmentFunction(_fragmentEntry);
}

} // namespace rhi::mtl

#endif
