#include "rhi/dx12/DXShader.hpp"
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

static std::string ToCsoSuffix(ShaderStage::Type type) {
    switch (type) {
        case ShaderStage::Vertex:   return ".vert.cso";
        case ShaderStage::Fragment: return ".frag.cso";
        case ShaderStage::Geometry: return ".geom.cso";
        case ShaderStage::Compute:  return ".comp.cso";
    }
    return ".cso";
}

// 三级查找（对齐 VKShader::LocateSpv 模式，产物为 dx_shaders 目标生成的
// build/res/DX12/<dir>/<name>.<stage>.cso）：
// 1) 直接给 .cso：原样返回；
// 2) 经 RESOURCE_DIR 推断 build/res/DX12 下同名产物；
// 3) 兜底：与源码同目录追加后缀。
static std::string LocateCso(const std::string& glslSource, ShaderStage::Type type) {
    namespace fs = std::filesystem;
    const std::string suffix = ToCsoSuffix(type);
    if (glslSource.size() > 4 &&
        glslSource.compare(glslSource.size() - 4, 4, ".cso") == 0)
        return glslSource;
    fs::path srcNoExt(glslSource);
    while (srcNoExt.has_extension()) srcNoExt.replace_extension();
    fs::path resRoot(RESOURCE_DIR);
    resRoot = resRoot.lexically_normal();
    if (resRoot.is_relative())
        resRoot = fs::absolute(resRoot);
    fs::path buildDx = resRoot.parent_path() / "build" / "res" / "DX12";
    fs::path rel = srcNoExt.lexically_relative(resRoot / "DX12");
    // native() 在 MSVC 下是 wstring，不能与 char 字面量比较；统一转窄字符串
    const std::string relStr = rel.string();
    if (!rel.empty() && !rel.is_absolute() &&
        relStr.rfind("..", 0) != 0) {
        fs::path candidate = buildDx / rel;
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

static std::string ListExistingCsos() {
    namespace fs = std::filesystem;
    fs::path resRoot(RESOURCE_DIR);
    resRoot = resRoot.lexically_normal();
    if (resRoot.is_relative())
        resRoot = fs::absolute(resRoot);
    fs::path buildDx = resRoot.parent_path() / "build" / "res" / "DX12";
    std::error_code ec;
    if (!fs::is_directory(buildDx, ec)) return "(no build/res/DX12)";
    std::string out;
    int n = 0;
    for (fs::recursive_directory_iterator it(buildDx, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec || n >= 16) { out += ", ..."; break; }
        const auto& p = it->path();
        const std::string pStr = p.string();   // native() 为 wstring，先转窄字符串再比较
        if (pStr.size() > 4 && pStr.compare(pStr.size() - 4, 4, ".cso") == 0) {
            out += out.empty() ? p.lexically_relative(buildDx).string()
                               : ", " + p.lexically_relative(buildDx).string();
            ++n;
        }
    }
    return out.empty() ? "(none)" : out;
}

std::string DXShader::FindCso(const std::string& sourcePath, ShaderStage::Type type) {
    return LocateCso(sourcePath, type);
}

bool DXShader::compile(const std::vector<ShaderStage>& stages) {
    _blobs.clear();
    _log.clear();
    for (const auto& st : stages) {
        std::string csoPath = st.sourceIsSPIRV ? st.source : LocateCso(st.source, st.type);
        std::ifstream f(csoPath, std::ios::binary);
        if (!f) {
            _log = "DXShader: cannot open " + csoPath + " (existing under build/res/DX12: " +
                   ListExistingCsos() + ")";
            LOGE("{}", _log);
            return false;
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // DXIL 容器按 32 位字组织，长度非 4 倍数必是损坏/误取文件
        if (bytes.empty() || bytes.size() % 4 != 0) {
            _log = "DXShader: invalid DXIL size for " + csoPath;
            LOGE("{}", _log);
            return false;
        }
        ComPtr<ID3DBlob> blob;
        DX_CHECK(D3DCreateBlob(bytes.size(), &blob), "D3DCreateBlob");
        if (!blob.Get()) {
            _log = "DXShader: D3DCreateBlob failed for " + csoPath;
            LOGE("{}", _log);
            return false;
        }
        std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
        _blobs.emplace(st.type, std::move(blob));
    }
    return !_blobs.empty();
}

ComPtr<ID3DBlob> DXShader::moduleFor(ShaderStage::Type type) const {
    auto it = _blobs.find(type);
    if (it == _blobs.end() || !it->second.Get()) return {};
    ComPtr<ID3DBlob> out;
    out.ptr = it->second.Get();
    out.ptr->AddRef();
    return out;
}

} // namespace rhi
