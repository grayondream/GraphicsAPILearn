#include "rhi/dx11/DX11Backend.hpp"
#include "base/AppDirs.hpp"
#include "base/Log.hpp"
#include <d3dcompiler.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#ifndef RESOURCE_DIR
#define RESOURCE_DIR "res"
#endif

namespace rhi {

// 多配置构建目录：从 exe 自身位置向上找含 res/<Backend> 产物树的目录
// （exe 位于 <cfg>/src[/Release]/，资源树在 <cfg>/res/），任意命名的
// 配置目录均可无缝运行（与 DXShader/VKShader 同一约定）
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

static std::string ToFxcSuffix(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return ".vert.fxc";
        case ShaderStage::Fragment: return ".frag.fxc";
        case ShaderStage::Geometry: return ".geom.fxc";
        case ShaderStage::Compute:  return ".comp.fxc";
    }
    return ".fxc";
}

// 三级查找（对齐 DXShader::LocateCso 模式，产物为 dx11_shaders 目标生成的
// build/res/DX11/<dir>/<name>.<stage>.fxc）：
// 1) 直接给 .fxc：原样返回；
// 2) 经 RESOURCE_DIR 推断 build/res/DX11 下同名产物；
// 3) 兜底：与源码同目录追加后缀。
static std::string LocateFxc(const std::string& glslSource, ShaderStage::Type type) {
    namespace fs = std::filesystem;
    const std::string suffix = ToFxcSuffix(type);
    if (glslSource.size() > 4 &&
        glslSource.compare(glslSource.size() - 4, 4, ".fxc") == 0)
        return glslSource;
    fs::path srcNoExt(glslSource);
    while (srcNoExt.has_extension()) srcNoExt.replace_extension();
    fs::path resRoot(RESOURCE_DIR);
    resRoot = resRoot.lexically_normal();
    if (resRoot.is_relative())
        resRoot = fs::absolute(resRoot);
    fs::path rel = srcNoExt.lexically_relative(resRoot / "DX11");
    // native() 在 MSVC 下是 wstring，不能与 char 字面量比较；统一转窄字符串
    const std::string relStr = rel.string();
    if (!rel.empty() && !rel.is_absolute() &&
        relStr.rfind("..", 0) != 0) {
        for (const auto& root : ArtifactRoots("DX11")) {
            fs::path candidate = root / rel;
            candidate += suffix;
            if (fs::exists(candidate))
                return candidate.string();
        }
        // 兼容旧布局：仓库根固定 build/ 目录
        fs::path legacyRoot = resRoot.parent_path() / "build" / "res" / "DX11";
        fs::path candidate = legacyRoot / rel;
        candidate += suffix;
        if (fs::exists(candidate))
            return candidate.string();
    }
    fs::path fallback = srcNoExt;
    fallback += suffix;
    if (fs::exists(fallback))
        return fallback.string();
    return glslSource;
}

static std::string ListExistingFxcs() {
    namespace fs = std::filesystem;
    fs::path resRoot(RESOURCE_DIR);
    resRoot = resRoot.lexically_normal();
    if (resRoot.is_relative())
        resRoot = fs::absolute(resRoot);
    fs::path buildDx = resRoot.parent_path() / "build" / "res" / "DX11";
    std::error_code ec;
    if (!fs::is_directory(buildDx, ec)) return "(no build/res/DX11)";
    std::string out;
    int n = 0;
    for (fs::recursive_directory_iterator it(buildDx, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec || n >= 16) { out += ", ..."; break; }
        const auto& p = it->path();
        const std::string pStr = p.string();   // native() 为 wstring，先转窄字符串再比较
        if (pStr.size() > 4 && pStr.compare(pStr.size() - 4, 4, ".fxc") == 0) {
            out += out.empty() ? p.lexically_relative(buildDx).string()
                               : ", " + p.lexically_relative(buildDx).string();
            ++n;
        }
    }
    return out.empty() ? "(none)" : out;
}

std::string DX11Shader::FindFxc(const std::string& sourcePath, ShaderStage::Type type) {
    return LocateFxc(sourcePath, type);
}

bool DX11Shader::compile(const std::vector<ShaderStage>& stages) {
    _blobs.clear();
    _log.clear();
    for (const auto& st : stages) {
        std::string fxcPath = st.sourceIsSPIRV ? st.source : LocateFxc(st.source, st.type);
        std::ifstream f(fxcPath, std::ios::binary);
        if (!f) {
            _log = "DX11Shader: cannot open " + fxcPath + " (existing under build/res/DX11: " +
                   ListExistingFxcs() + ")";
            LOGE("{}", _log);
            return false;
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // SM5.0 字节码容器按 32 位字组织，长度非 4 倍数必是损坏/误取文件
        if (bytes.empty() || bytes.size() % 4 != 0) {
            _log = "DX11Shader: invalid bytecode size for " + fxcPath;
            LOGE("{}", _log);
            return false;
        }
        ComPtr<ID3DBlob> blob;
        DX11_CHECK(D3DCreateBlob(bytes.size(), &blob), "D3DCreateBlob");
        if (!blob.Get()) {
            _log = "DX11Shader: D3DCreateBlob failed for " + fxcPath;
            LOGE("{}", _log);
            return false;
        }
        std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
        _blobs.emplace(st.type, std::move(blob));
    }
    return !_blobs.empty();
}

ComPtr<ID3DBlob> DX11Shader::moduleFor(ShaderStage::Type type) const {
    auto it = _blobs.find(type);
    if (it == _blobs.end() || !it->second.Get()) return {};
    ComPtr<ID3DBlob> out;
    out.ptr = it->second.Get();
    out.ptr->AddRef();
    return out;
}

} // namespace rhi
