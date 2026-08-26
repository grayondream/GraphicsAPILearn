#pragma once
#include "rhi/core/IRenderer.hpp"

#if defined(_WIN32)
#include "rhi/dx11/DX11Header.hpp"
#include "rhi/core/ISwapchain.hpp"
#include "rhi/core/IShader.hpp"
#include "rhi/core/IPipeline.hpp"
#include "rhi/core/IBuffer.hpp"
#include "rhi/core/ITexture2D.hpp"
#include "rhi/core/ITexture3D.hpp"
#include "rhi/core/IRenderTarget.hpp"
#include <array>
#include <map>
#include <unordered_map>
#include <vector>

namespace rhi {

class ISurface;

struct DX11ImGuiInitInfo {
    ID3D11Device* device{nullptr};
    ID3D11DeviceContext* context{nullptr};
};

std::shared_ptr<IRenderer> createDX11Renderer();

// ImGui overlay 初始化信息桥（Task 6）：下转型读取 device/context 句柄供
// ImGui_ImplDX11_Init 使用；renderer 非 DX11 或设备未就绪时返回 false
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
    // 窗口深度纹理资源（blitFramebuffer 的 Depth→窗口路径消费；CopyResource 需
    // 与离屏深度同格式——两侧统一 TYPELESS 族 R24G8_TYPELESS）
    ID3D11Texture2D* depthResource() { return _depth.Get(); }
    // 已获取槽位的 backbuffer 裸纹理（dumpFrame 读回源；未获取槽位返回 nullptr）
    ID3D11Texture2D* backBuffer(uint32_t index);

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

// ---- 内部 blit 能力接口（Task 5，对照 DX12 IDXBlitContext）----
// 实现于 DX11Renderer（工厂创建纹理时注入）。mipdown=Gather 角点等权盒平均的
// 全屏三角形降采样（_internal/mipdown*.hlsl），对齐 vkCmdBlitImage linear 的
// 2:1 盒式语义（GL generateMipmap 同为盒式）；着色器对象按格式无关缓存（D3D11
// 无 PSO/根签名，SRV/RTV 视图由实现侧按目标纹理懒建）。
class DX11Texture2D;
class DX11Texture3D;
class IDX11BlitContext {
public:
    virtual ~IDX11BlitContext() = default;
    // 2D 纹理 mip 链降采样（mipdown.frag：Texture2D 源）
    virtual bool Mipdown2D(DX11Texture2D* tex) = 0;
    // cubemap mip 链逐面降采样（mipdown_array.frag：TEXTURE2DARRAY 单面单级源视图；
    // 与 Texture2D 声明混用属视图类型不匹配 UB，须用数组变体——同 DX12 口径）
    virtual bool MipdownCube(DX11Texture3D* tex) = 0;
};

// ---- 2D 纹理（实现于 DX11Texture2D.cpp）----
// - 上传：STAGING 纹理 Map+行拷贝 → CopySubresourceRegion 写 DEFAULT 资源 mip0
//   （brief 指定路径）；generateMipmap=true 时分配 mip 链并经注入的 IDX11BlitContext
//   走 Gather 盒平均降采样（Task 5 对齐 DX12/VK/GL 盒式语义；blit 缺失时回退
//   D3D11 内建 GenerateMips 兜底）；非 filterable 格式（RGBA32F 等）钳 mip0 并告警
//   （对齐 DX12 "mipgen 不可用钳 mip0" 的降级语义）；
// - createEmpty：颜色=RT|SRV 绑定；深度=TYPELESS 资源族 + BIND_DEPTH_STENCIL|
//   BIND_SHADER_RESOURCE，DSV/SRV 视图各取 typed 格式（R32_TYPELESS→D32_FLOAT/
//   R32_FLOAT、R24G8_TYPELESS→D24_UNORM_S8_UINT/R24_UNORM_X8_TYPELESS）——
//   D3D11 深度+采样双绑定必须 TYPELESS 资源，Dx11FormatOf 的 typed 直传仅适用于
//   纯 DSV 用途，此处按视图口径展开（同 DX12 srvFormat 决策）；
// - SRV 在 init 成功时创建一次（覆盖全 mip 链），bindTexture 把它写入 Renderer
//   共享槽位 t<unit+1>（槽 0 预留 ImGui）。
class DX11Texture2D : public ITexture2D {
public:
    // device/context 归 Renderer 所有，此处仅借用
    DX11Texture2D(ID3D11Device* device, ID3D11DeviceContext* context);
    ~DX11Texture2D() override;

    bool init(const TextureDataView2D& data) override;                  // 旧签名：默认 RGBA8 desc
    bool init(const TextureDesc& desc, const TextureDataView2D& data) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit) override {}                            // 绑定=Renderer 写共享槽位
    void* handle() override { return _texture.ptr; }
    bool valid() const override { return _valid; }
    void release() override;

    // ---- Renderer 访问器 ----
    ID3D11ShaderResourceView* srv() const { return _srv.Get(); }
    UINT mipLevels() const { return _mipLevels; }
    bool isMsaa() const { return _msaa; }                               // RTV-only，不可建 SRV
    DXGI_FORMAT storageFormat() const { return _format; }               // 资源存储格式（深度为 TYPELESS 族）
    // 采样语义（bind 路径取 f*3+w 采样器槽位；borderColor 仅白/黑可静态表达）
    const TextureDesc& samplerParams() const { return _params; }
    const std::array<float, 4>& borderColor() const { return _borderColor; }
    void setBorderColor(const float bc[4]);
    // mipgen blit 能力注入（Renderer 工厂 init 前调用；弱引用）
    void setBlitContext(IDX11BlitContext* ctx) { _blitCtx = ctx; }
    DXGI_FORMAT srvFormat() const { return _srvFormat; }                // typed 视图格式（mipgen 视图建用）

private:
    bool uploadAndGenMips(const TextureDesc& desc, const TextureDataView2D& data);

    ID3D11Device* _device{nullptr};
    ID3D11DeviceContext* _context{nullptr};

    Dx11ComPtr<ID3D11Texture2D> _texture{};
    Dx11ComPtr<ID3D11ShaderResourceView> _srv{};
    DXGI_FORMAT _format{DXGI_FORMAT_UNKNOWN};     // 资源存储格式（深度为 TYPELESS 族）
    DXGI_FORMAT _srvFormat{DXGI_FORMAT_UNKNOWN};  // SRV 视图 typed 格式
    UINT _mipLevels{1};
    int _width{0};
    int _height{0};
    bool _msaa{false};
    bool _valid{false};

    TextureDesc _params{};                      // 创建时的采样语义（filter/wrap）
    std::array<float, 4> _borderColor{{1.0f, 1.0f, 1.0f, 1.0f}};  // ClampToBorder 边框色
    IDX11BlitContext* _blitCtx{nullptr};        // 弱引用（Renderer 注入，mipgen 用）
};

// ---- 3D/立方体纹理（实现于 DX11Texture3D.cpp，Task 4）----
// - cubemap 承载：D3D11 无独立 cube 资源类型，用 Texture2D(ArraySize=6)+
//   MISC_TEXTURECUBE 建，SRV 取 TEXTURECUBE 维度（点光阴影深度图/天空盒同路径）；
//   深度格式按 TYPELESS 族建资源（Depth32F→R32_TYPELESS、D24S8→R24G8_TYPELESS），
//   DSV/SRV 视图阶段取 typed 格式（同 DXTexture3D::createEmpty 约定）；
// - initCube CPU 上传：逐面 STAGING Map → CopySubresourceRegion（子资源索引
//   =face*mipLevels）；generateMipmap=true 时 genCubeMipmaps 经注入的
//   IDX11BlitContext 逐面 Gather 盒平均降采样（mipdown_array，Task 5 对齐 DX12；
//   blit 缺失时回退 D3D11 内建 GenerateMips 兜底）；
// - 渲染目标接入：颜色 cube 提供 rtvFace(face,mip)、深度 cube 提供 dsvFace(face)
//   （TEXTURE2DARRAY 视图 FirstArraySlice=面），DX11RenderTarget::attachCubeFace
//   消费；真 3D 纹理（init 旧签名）走 ID3D11Texture3D，仅 RGBA8 上传。
class DX11Texture3D : public ITexture3D {
public:
    // device/context 归 Renderer 所有，此处仅借用
    DX11Texture3D(ID3D11Device* device, ID3D11DeviceContext* context);
    ~DX11Texture3D() override;

    bool init(const TextureDataView3D& data) override;                  // 真 3D 纹理：RGBA8 上传
    bool initCube(const TextureDesc& desc, const TextureDataView2D* faces) override;
    bool createEmpty(const TextureDesc& desc, int width, int height) override;
    void bind(unsigned int unit) override {}                            // 绑定=Renderer 写共享槽位
    void* handle() override;                                            // Texture2D(cube)/Texture3D
    bool valid() const override { return _valid; }
    void release() override;
    void genCubeMipmaps() override;

    // mipgen blit 能力注入（Renderer 工厂 init 前调用；弱引用）
    void setBlitContext(IDX11BlitContext* ctx) { _blitCtx = ctx; }

    // ---- Renderer / RT 访问器 ----
    ID3D11ShaderResourceView* srv() const { return _srv.Get(); }
    const TextureDesc& samplerParams() const { return _params; }
    DXGI_FORMAT dsvFormat() const { return _dsvFormat; }                // 深度 cube DSV typed 格式
    DXGI_FORMAT srvFormat() const { return _srvFormat; }                // typed 视图格式（mipgen 视图建用）
    UINT mipLevels() const { return _mipLevels; }
    bool isCube() const { return _cube; }
    bool isDepth() const { return _depth; }
    // 深度 cube 逐面 DSV（TEXTURE2DARRAY FirstArraySlice=face，懒建 6 槽）
    // 与颜色 cube 逐面 RTV（IBL capture 用，懒建 6*mips 槽）
    ID3D11DepthStencilView* dsvFace(int face);
    ID3D11RenderTargetView* rtvFace(int face, int mip);

private:
    ID3D11Device* _device{nullptr};
    ID3D11DeviceContext* _context{nullptr};

    Dx11ComPtr<ID3D11Texture2D> _texture{};     // cube 路径（ArraySize=6）
    Dx11ComPtr<ID3D11Texture3D> _texture3d{};   // 真 3D 路径（init 旧签名）
    Dx11ComPtr<ID3D11ShaderResourceView> _srv{};
    std::array<Dx11ComPtr<ID3D11DepthStencilView>, 6> _faceDsv{};
    std::vector<Dx11ComPtr<ID3D11RenderTargetView>> _faceRtv{};

    DXGI_FORMAT _format{DXGI_FORMAT_UNKNOWN};     // 资源存储格式（深度为 TYPELESS 族）
    DXGI_FORMAT _srvFormat{DXGI_FORMAT_UNKNOWN};  // SRV 视图 typed 格式
    DXGI_FORMAT _dsvFormat{DXGI_FORMAT_UNKNOWN};  // DSV 视图 typed 格式
    DXGI_FORMAT _rtvFormat{DXGI_FORMAT_UNKNOWN};  // 颜色 RTV 视图格式
    UINT _mipLevels{1};
    int _width{0};
    int _height{0};
    int _depthSlices{0};                 // init(3D) 的 z 维度（cube 恒 6）
    bool _cube{false};
    bool _depth{false};
    bool _mipsBindable{false};           // 资源带 BIND_RT+MISC_GENERATE_MIPS（GenerateMips 前置）
    IDX11BlitContext* _blitCtx{nullptr}; // 弱引用（Renderer 注入，mipgen 用）
    bool _valid{false};

    TextureDesc _params{};               // 创建时的采样语义（filter/wrap）
};

// ---- 最小管线（实现于 DX11Pipeline.cpp）----
// VS/PS(+GS)/InputLayout/Topology + RS/Blend/DS 状态对象缓存（Task 2）：
// 状态 setter 存成员，draw 时按 stateHash() 查懒建缓存并全量下发——DX11 即时
// 上下文无状态泄漏风险（对齐 DX12 "prepareDraw 自愈重设" 注释语义）。
// uniform 走显式 UBO（cbuffer b0），pipeline 侧 setter 一律 no-op（同 DX12/VK 约定）。
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
    // RS/Blend/DS 状态对象按 stateHash 懒建缓存后全量下发（RSSetState +
    // OMSetDepthStencilState(stencilRef) + OMSetBlendState），prepareDraw 每 draw 调用；
    // 返回 false=状态对象创建失败（不缓存不重试，调用方跳过本次 draw，对照
    // DXPipeline::pipelineFor 返回 nullptr 的语义）
    bool bindStates(ID3D11DeviceContext* ctx);
    // 状态指纹（与 DX12 同字段口径：混合/深度/模板/剔除/朝向/多态/拓扑；
    // 差异点：_stencilRef 参与键——DX11 无独立 ref 绑定通道，ref 变化必须换状态对象）
    uint32_t stateHash() const;
    int stencilRef() const { return _stencilRef; }

private:
    void ensureShaderObjects();   // VS/PS/GS 字节码 → D3D11 着色器对象（一次）

    // 状态对象三元组缓存值（生命周期随 pipeline，析构经 Dx11ComPtr 释放）
    struct StateObjects {
        Dx11ComPtr<ID3D11RasterizerState> rs{};
        Dx11ComPtr<ID3D11DepthStencilState> ds{};
        Dx11ComPtr<ID3D11BlendState> blend{};
    };
    // stateHash → 状态对象；未命中时按当前成员构建，任一创建失败返回 nullptr
    // 且不入缓存（避免 null 对象被永久复用；对照 DXPipeline::pipelineFor）
    StateObjects* statesFor(uint32_t hash);

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

    std::unordered_map<uint32_t, StateObjects> _stateCache{};
};

// ---- 离屏渲染目标（实现于 DX11RenderTarget.cpp，Task 4，对照 DXRenderTarget 接口面）----
// - 附件：颜色/深度按 FramebufferDesc 内部建 DX11Texture2D（颜色 BIND_RT|SRV；
//   深度 TYPELESS 族 BIND_DSV|SRV typed 视图；MSAA 颜色 RTV-only）并各建 RTV/DSV
//   视图（TEXTURE2D/TEXTURE2DMS 维度）；colorTexture2D(i)/depthTexture2D() 直接
//   返回内部纹理（阴影采样/后处理 quad 消费 SRV）；
// - cube 挂接：attachCubeFace/attachDepthCube 仅记录挂接状态（延迟生效），OM 目标
//   由 Renderer flushOmTargets 在每 draw 前装配——深度 cube 面取 DX11Texture3D::
//   dsvFace(face)（TEXTURE2DARRAY 切片），颜色 cube 面取 rtvFace(face,mip)；纯深度
//   pass 合法（OMSetRenderTargets(0,nullptr,dsv)）；
// - D3D11 即时上下文无资源状态/屏障概念：DX12 的 BeginPass/EndPass 往返在此坍缩为
//   "setRenderTarget 必置 pending → 每 draw 前 flushOmTargets 完整重绑+清屏"，
//   "重复激活走完整往返"语义由 setRenderTarget 恒置 pending 保证。
class DX11RenderTarget : public IRenderTarget {
public:
    // device/context 归 Renderer 所有，此处仅借用
    DX11RenderTarget(ID3D11Device* device, ID3D11DeviceContext* context);
    ~DX11RenderTarget() override;

    bool create(int width, int height) override;                          // 旧签名：RGBA8 + Depth24Stencil8
    bool create(const FramebufferDesc& desc) override;
    bool attachCubeFace(ITexture3D* cube, int face, int mip = 0) override;
    bool attachDepthCube(ITexture3D* cube, int mip = 0) override;
    bool bind() override { return true; }                                 // 绑定由 Renderer 统一编排
    bool unbind() override { return true; }
    void* colorTexture() override;
    ITexture2D* colorTexture2D(int attachment = 0) override;
    ITexture2D* depthTexture2D() override;
    bool resolveTo(IRenderTarget& dst) override;                          // MSAA resolve 走 blitFramebuffer
    void* handle() override;                                              // 颜色 0 纹理（无颜色返回 nullptr）
    void release() override;

    // ---- Renderer 协作 ----
    bool valid() const { return _valid; }
    uint32_t colorCount() const;                                          // 当前生效颜色目标数（纯深度 pass=0）
    // 当前生效 OM 句柄组装：内部附件 RTV 数组或 cube 面（attachCubeFace 延迟生效，
    // Renderer flushOmTargets 每 draw 前消费）；FillRtvs 返回实际写入数
    UINT FillRtvs(ID3D11RenderTargetView** out, UINT maxCount);
    ID3D11DepthStencilView* ActiveDsv();                                  // 内部深度或深度 cube 当前面
    bool hasDepthAttachment() const;
    void renderDims(int& w, int& h) const;                                // viewport 兜底尺寸（cube 面=mip 尺寸）
    // 清屏：当前生效颜色 RTV ×N + 深度/模板 DSV（stencil 标志按格式有无裁剪）
    void ClearAll(ID3D11DeviceContext* ctx, const std::array<float, 4>& cc);

private:
    bool depthHasStencil() const;

    ID3D11Device* _device{nullptr};
    ID3D11DeviceContext* _context{nullptr};

    int _width{0};
    int _height{0};
    UINT _samples{1};

    std::vector<std::shared_ptr<DX11Texture2D>> _colors{};     // 非 MSAA 颜色（可采样）
    std::vector<std::shared_ptr<DX11Texture2D>> _msaaColors{}; // MSAA 颜色（RTV-only）
    std::shared_ptr<DX11Texture2D> _depth{};                   // 内部深度（可空）
    std::vector<Dx11ComPtr<ID3D11RenderTargetView>> _rtvs{};   // 与 [_colors..., _msaaColors...] 对齐
    Dx11ComPtr<ID3D11DepthStencilView> _dsv{};
    DXGI_FORMAT _dsvFormat{DXGI_FORMAT_UNKNOWN};               // 内部深度 typed 格式（Clear 标志裁剪）

    // cube 挂接状态（弱引用，App 持有纹理生命周期；face<0=仅 attachDepthCube 未选面）
    DX11Texture3D* _cube{nullptr};
    int _face{-1};
    int _mip{0};
    bool _cubeIsDepth{false};
    bool _valid{false};
};

} // namespace rhi

#else
// 非 Windows 平台无 D3D11 运行时：工厂返回空指针，AppHost 判空后干净退出
// （`-b dx11` 报无 D3D11 运行时 rc=1 为预期行为），Linux 构建零影响
namespace rhi {
inline std::shared_ptr<IRenderer> createDX11Renderer() { return nullptr; }
} // namespace rhi

#endif // defined(_WIN32)
