#pragma once
#include "rhi/core/IRenderer.hpp"

#if defined(_WIN32)
#include "rhi/dx11/DX11Header.hpp"
#include "rhi/core/ISwapchain.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include <map>

namespace rhi {

class ISurface;

struct DX11ImGuiInitInfo {
    ID3D11Device* device{nullptr};
    ID3D11DeviceContext* context{nullptr};
};

std::shared_ptr<IRenderer> createDX11Renderer();

// ImGui overlay 初始化信息桥（Task 6 消费）：本任务恒返回 false
bool GetDX11ImGuiInitInfo(const std::shared_ptr<IRenderer>& renderer, DX11ImGuiInitInfo& out);

// ---- 交换链（实现于 DX11Swapchain.cpp）----
// DXGI flip-model 交换链：CreateDXGIFactory1 → D3D11CreateDevice 由 Renderer 完成，
// 本类只持 IDXGISwapChain1 与 backbuffer RTV / 窗口深度 DSV 的生命周期。
// FLIP_DISCARD 创建失败时回退传统 DISCARD（brief 约定）；呈现编排（dumpFrame →
// Present(1,0)）由 DXRenderer::present 完成，本类只管 DXGI 对象与视图。
class DX11Swapchain : public ISwapchain {
public:
    ~DX11Swapchain() override;

    bool init(ID3D11Device* device, const std::shared_ptr<ISurface>& surface);
    void shutdown();

    bool present() override;   // Present(1,0)：vsync 对齐 VK FIFO/GL swapInterval
    void resize(int width, int height) override;
    void* handle() override;   // IDXGISwapChain1*

    bool initialized() const { return _initialized; }
    uint32_t bufferCount() const { return kBufferCount; }
    uint32_t currentIndex() const;   // GetCurrentBackBufferIndex（IDXGISwapChain3；不可用时恒 0）
    int width() const { return _width; }
    int height() const { return _height; }
    DXGI_FORMAT colorFormat() const { return DXGI_FORMAT_B8G8R8A8_UNORM; }
    // 惰性获取指定 backbuffer 的 RTV：flip 模型下 DXGI 按 Present 进度惰性分配
    // backbuffer，未分配槽位的 GetBuffer 返回 DXGI_ERROR_INVALID_CALL（本机实测：
    // 建链初期仅 currentIndex 槽可取），故按需获取并缓存；失败返回 nullptr 由
    // 调用方跳过本帧（后续帧自动重试）
    ID3D11RenderTargetView* acquireRtv(uint32_t index);
    ID3D11DepthStencilView* dsv();

private:
    bool createSizeDependent(int width, int height);   // 尽力预取各槽 RTV+深度 DSV
    void destroySizeDependent();

    static constexpr UINT kBufferCount = 2;

    ID3D11Device* _device{nullptr};
    Dx11ComPtr<IDXGIFactory2> _factory;
    Dx11ComPtr<IDXGISwapChain1> _swapchain;
    // GetCurrentBackBufferIndex 需 DXGI 1.4 接口（Win10 起可用；FLIP 模型本身即
    // Win10 特性，QI 失败时 currentIndex 回退 0）
    Dx11ComPtr<IDXGISwapChain3> _swapchain3;
    Dx11ComPtr<ID3D11Texture2D> _buffers[kBufferCount];
    Dx11ComPtr<ID3D11RenderTargetView> _rtv[kBufferCount];
    Dx11ComPtr<ID3D11Texture2D> _depth;
    Dx11ComPtr<ID3D11DepthStencilView> _dsv;
    int _width{0};
    int _height{0};
    bool _initialized{false};
};

// ---- 着色器产物装载（实现于 DX11Shader.cpp）----
// .fxc 三级查找（对齐 DXShader::LocateCso 模式，产物为 dx11_shaders 目标生成的
// build/res/DX11/<dir>/<name>.<stage>.fxc）：直接路径 / RESOURCE_DIR 推导 /
// 与源码同目录兜底。SM5.0 字节码按 4 字节整数容器组织，长度非 4 倍数视为损坏。
class DX11Shader : public IShader {
public:
    bool compile(const std::vector<ShaderStage>& stages) override;
    std::string getLog() const override { return _log; }
    bool valid() const override { return !_blobs.empty(); }

    bool hasStage(ShaderStage::Type type) const { return _blobs.count(type) != 0; }
    // 返回独立引用（AddRef 拷贝），调用方经 Dx11ComPtr 释放
    Dx11ComPtr<ID3DBlob> moduleFor(ShaderStage::Type type) const;

    // 产物定位约定复用入口（与 compile 同一套三级查找）
    static std::string FindFxc(const std::string& sourcePath, ShaderStage::Type type);

private:
    std::map<ShaderStage::Type, Dx11ComPtr<ID3DBlob>> _blobs{};
    std::string _log{};
};

// ---- 最小管线（实现于 DX11Pipeline.cpp）----
// 本任务只 VS/PS(+GS)/InputLayout/Topology；深度/模板/混合等状态对象 Task 3 补全，
// 届时状态 setter 存储的成员将参与 D3D11 状态对象构建。uniform 走显式 UBO
// （cbuffer b0），pipeline 侧 setter 一律 no-op（同 DX12/VK 约定）。
class DX11Pipeline : public IPipeline {
public:
    // device 归 Renderer 所有，此处仅借用；InputLayout 在构造期由 VS 字节码创建
    DX11Pipeline(ID3D11Device* device, const VertexLayout& layoutIn,
                 std::shared_ptr<IShader> shader);
    ~DX11Pipeline() override = default;

    void use() override {}   // GL use() 绑 program+VAO；DX 状态在 draw 时由 Renderer 装配
    void* handle() override { return _inputLayout.ptr; }

    // 与 VK/DX12 一致：uniform 走显式 UBO，pipeline 侧 no-op 返回 false
    bool setUniform(const std::string&, bool) override { return false; }
    bool setUniform(const std::string&, int) override { return false; }
    bool setUniform(const std::string&, float) override { return false; }
    bool setUniform(const std::string&, const float*, int) override { return false; }
    bool setUniform(const std::string&, const float*, int, int) override { return false; }
    bool setUniformMatrix(const std::string&, const float*, int, int) override { return false; }
    void bindUniformBlock(uint32_t) override {}   // b0 槽位硬编码

    // 渲染状态 setter：只写成员（Task 3 状态对象化后参与构建 D3D11 状态对象）
    void setDepthTest(bool enable) override { _depthTest = enable; }
    void setCullMode(bool enable, int face) override { _cullEnable = enable; _cullFace = static_cast<CullFace>(face); }
    void setBlend(bool enable) override { _blend = enable; }
    void setDepthFunc(CompareFunc func) override { _depthFunc = func; }
    void setDepthMask(bool write) override { _depthMask = write; }
    void setStencilTest(bool enable) override { _stencilTest = enable; }
    void setStencilFunc(CompareFunc func, int ref, unsigned mask) override { _stencilFunc = func; _stencilRef = ref; _stencilCompareMask = mask; }
    void setStencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) override { _stencilFail = sfail; _stencilDepthFail = dpfail; _stencilPass = dppass; }
    void setStencilMask(unsigned mask) override { _stencilWriteMask = mask; }
    void setBlendFunc(BlendFactor src, BlendFactor dst) override { _blendSrc = src; _blendDst = dst; }
    void setCullFaceEnable(bool enable) override { _cullEnable = enable; }
    void setCullFace(CullFace face) override { _cullFace = face; }
    void setFrontFace(bool ccw) override { _frontFaceCCW = ccw; }
    void setPolygonMode(PolygonMode mode) override { _polygonMode = mode; }
    void setPointSizeProgramEnable(bool enable) override { _pointSizeEnable = enable; }
    void setMultisample(bool enable) override { _multisample = enable; }
    void setPrimitiveType(PrimitiveType type) override { _primitive = type; }
    PrimitiveType primitiveType() const override { return _primitive; }

    // Renderer 绑定顶点缓冲时按 binding 取 stride（IBuffer 无法自述步长）
    const VertexLayout& layout() const { return _layout; }

    // 缓冲绑定发生在 draw 时（Renderer 持 layout stride，同 DX12 prepareDraw）
    void setVertexBuffer(const std::shared_ptr<IBuffer>&, uint32_t) override {}
    void setIndexBuffer(const std::shared_ptr<IBuffer>&) override {}

    bool valid() const { return _inputLayout.Get() && _shader && _shader->valid(); }
    // IASetInputLayout + VSSetShader/PSSetShader(+GSSetShader)，prepareDraw 消费。
    // 非 const：着色器对象（ID3D11VertexShader 等）按字节码懒创建并缓存
    void bindShaders(ID3D11DeviceContext* ctx);
    // Task 3 状态对象化前模板参考值先经成员透出（OMSetStencilRef 属即时上下文状态）
    int stencilRef() const { return _stencilRef; }

private:
    void ensureShaderObjects();   // VS/PS/GS 字节码 → D3D11 着色器对象（一次）

    ID3D11Device* _device{nullptr};
    Dx11ComPtr<ID3D11InputLayout> _inputLayout{};
    Dx11ComPtr<ID3D11VertexShader> _vs{};
    Dx11ComPtr<ID3D11PixelShader> _ps{};
    Dx11ComPtr<ID3D11GeometryShader> _gs{};
    bool _shadersReady{false};
    VertexLayout _layout{};
    std::shared_ptr<DX11Shader> _shader{};

    // 状态全集镜像 DXPipeline 成员与默认值（Task 3 状态对象化输入）
    bool _depthTest{false};
    bool _depthMask{true};
    CompareFunc _depthFunc{CompareFunc::Less};
    bool _cullEnable{false};
    CullFace _cullFace{CullFace::Back};
    bool _frontFaceCCW{true};
    bool _stencilTest{false};
    CompareFunc _stencilFunc{CompareFunc::Always};
    int _stencilRef{0};
    unsigned _stencilCompareMask{0xFF};
    StencilOp _stencilFail{StencilOp::Keep};
    StencilOp _stencilDepthFail{StencilOp::Keep};
    StencilOp _stencilPass{StencilOp::Keep};
    unsigned _stencilWriteMask{0xFF};
    bool _blend{false};
    BlendFactor _blendSrc{BlendFactor::SrcAlpha};
    BlendFactor _blendDst{BlendFactor::OneMinusSrcAlpha};
    PolygonMode _polygonMode{PolygonMode::Fill};
    bool _pointSizeEnable{false};
    bool _multisample{false};
    PrimitiveType _primitive{PrimitiveType::TriangleList};
};

} // namespace rhi

#else
// 非 Windows 平台无 D3D11 运行时：工厂返回空指针，AppHost 判空后干净退出
// （`-b dx11` 报无 D3D11 运行时 rc=1 为预期行为），Linux 构建零影响
namespace rhi {
inline std::shared_ptr<IRenderer> createDX11Renderer() { return nullptr; }
} // namespace rhi

#endif // defined(_WIN32)
