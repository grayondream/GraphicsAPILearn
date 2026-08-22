// tools/dx12_spike/spike.cpp —— WSL D3D12 阶段0 技术验证（spike）
// 验证项：设备创建 / Downlevel Present / 完整 DXGI 运行时探测（dxc 见 Step4 脚本）
#include <wsl/winadapter.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <directx/dxcore.h>
#include <directx/dxcore_interface.h>
#include <directx/dxgiformat.h>
#include <dxguids/dxguids.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>

#define DX_CHECK(hr, msg) do { if (FAILED(hr)) { printf("FAIL %s hr=0x%08X\n", msg, (unsigned)(hr)); return 1; } } while (0)

// ---- Downlevel present 接口 ----
// WSL 的 libd3d12.so 实现了与 D3D12onWin7 ID3D12CommandQueueDownlevel 相同 IID 的呈现接口
// （在 libd3d12.so/libd3d12core.so 二进制中确认该 GUID），但该接口声明未随 DirectX-Headers 分发，
// 此处自行声明。第 5 参存在两种 ABI 变体：
//   Win7 官方版: Flags (UINT，经 r8 寄存器)
//   brief 调研版: DestRect (16 字节结构，经 XMM 寄存器)
// 两者前 4 参一致、错配不会段错误，运行时依次实测并以结果为准。
struct D2D1_RECT_F { float left; float top; float right; float bottom; };
enum D3D12_DOWNLEVEL_PRESENT_FLAGS { D3D12_DOWNLEVEL_PRESENT_FLAG_NONE = 0 };

// {38A8C5EF-7CCB-4E81-914F-A6E9D072C494}
static constexpr GUID kIID_ID3D12CommandQueueDownlevel =
    {0x38a8c5ef, 0x7ccb, 0x4e81, {0x91, 0x4f, 0xa6, 0xe9, 0xd0, 0x72, 0xc4, 0x94}};

struct ID3D12CommandQueueDownlevelRect : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList* openCmdList,
                                              ID3D12Resource* sourceTex2D,
                                              HWND window,
                                              D2D1_RECT_F destRect) = 0;
};
struct ID3D12CommandQueueDownlevelFlags : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList* openCmdList,
                                              ID3D12Resource* sourceTex2D,
                                              HWND window,
                                              D3D12_DOWNLEVEL_PRESENT_FLAGS flags) = 0;
};

static HWND GetHwnd(GLFWwindow* win) { return (HWND)(uintptr_t)glfwGetX11Window(win); }

int main() {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11); // WSLg 下强制 XWayland，glfwGetX11Window 需要
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* win = glfwCreateWindow(320, 240, "dx12-spike", nullptr, nullptr);
    if (!win) { printf("FAIL create glfw window\n"); return 1; }
    printf("x11 window=0x%lx\n", (unsigned long)(uintptr_t)GetHwnd(win));

    // ---- 设备创建 ----
    // WSL 关键差异：必须经 DXCore 显式枚举适配器再传入 D3D12CreateDevice；
    // 传 nullptr 的默认枚举路径在 WSL shim 上返回 DXGI_ERROR_UNSUPPORTED(0x887A0004)。
    IDXCoreAdapterFactory* dxFac = nullptr;
    DX_CHECK(DXCoreCreateAdapterFactory(IID_PPV_ARGS(&dxFac)), "DXCoreCreateAdapterFactory");
    const GUID attrD3D12 = DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS;
    IDXCoreAdapterList* adList = nullptr;
    DX_CHECK(dxFac->CreateAdapterList(1, &attrD3D12, IID_PPV_ARGS(&adList)), "CreateAdapterList");
    printf("dxcore d3d12 adapters=%u\n", adList->GetAdapterCount());
    if (adList->GetAdapterCount() == 0) { printf("FAIL no D3D12 adapter\n"); return 1; }
    IDXCoreAdapter* adapter = nullptr;
    DX_CHECK(adList->GetAdapter(0, IID_PPV_ARGS(&adapter)), "GetAdapter");

    ID3D12Device* dev = nullptr;
    HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev));
    DX_CHECK(hr, "D3D12CreateDevice(adapter, FL12_0)");
    D3D12_FEATURE_DATA_ARCHITECTURE arch{};
    if (SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch))))
        printf("arch UMA=%d ccUMA=%d\n", arch.UMA, arch.CacheCoherentUMA);
    printf("device OK\n");

    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* queue = nullptr;
    DX_CHECK(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)), "CreateCommandQueue");

    // ---- 检测 A：Downlevel ----
    void* dlVoid = nullptr;
    const bool downlevel = SUCCEEDED(queue->QueryInterface(kIID_ID3D12CommandQueueDownlevel, &dlVoid));
    printf("downlevel=%d\n", (int)downlevel);

    // ---- 检测 B：完整 DXGI ----
    // WSL 环境无独立 DXGI 运行时（ldconfig 无 libdxgi），运行时探测给出结论；
    // 若未来存在实现则打印符号地址供后续深入。
    int dxgi = 0;
    {
        void* h = dlopen("libdxgi.so.1", RTLD_NOW);
        if (!h) h = dlopen("libdxgi.so", RTLD_NOW);
        if (!h) {
            printf("no libdxgi (%s)\n", dlerror());
        } else {
            using PFN_CreateDXGIFactory2 = HRESULT (*)(unsigned, const void*, void**);
            auto create = (PFN_CreateDXGIFactory2)dlsym(h, "CreateDXGIFactory2");
            printf("libdxgi loaded, CreateDXGIFactory2=%p\n", (void*)create);
            dxgi = create != nullptr;
        }
        printf("dxgiFactory=%d\n", dxgi);
    }

    // ---- 清屏一帧并经 Downlevel 呈现（成功则窗口应变红 2 秒）----
    if (downlevel && dlVoid) {
        ID3D12CommandAllocator* al = nullptr;
        DX_CHECK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al)), "alloc");
        ID3D12GraphicsCommandList* cl = nullptr;
        DX_CHECK(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, nullptr, IID_PPV_ARGS(&cl)), "cl");

        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
        CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 320, 240);
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        ID3D12Resource* rt = nullptr;
        D3D12_CLEAR_VALUE cv{}; cv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        const float initRed[4] = {1, 0, 0, 1};
        memcpy(cv.Color, initRed, sizeof(float) * 4);
        DX_CHECK(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                              D3D12_RESOURCE_STATE_RENDER_TARGET, &cv,
                                              IID_PPV_ARGS(&rt)), "rt");

        ID3D12DescriptorHeap* dh = nullptr;
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
        DX_CHECK(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dh)), "rtv heap");
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = dh->GetCPUDescriptorHandleForHeapStart();
        dev->CreateRenderTargetView(rt, nullptr, rtv);

        const FLOAT red[4] = {1, 0, 0, 1};
        cl->ClearRenderTargetView(rtv, red, 0, nullptr);
        cl->Close();
        ID3D12CommandList* cls[] = {cl};
        queue->ExecuteCommandLists(1, cls);

        // Present 语义实测：官方签名参数名为 pOpenCommandList；brief 先 Close 后传入，
        // 两种顺序都试，以运行结果为准。
        auto* dlRect = reinterpret_cast<ID3D12CommandQueueDownlevelRect*>(dlVoid);
        D2D1_RECT_F rect{0, 0, 320, 240};
        HRESULT hrRect = dlRect->Present(cl, rt, GetHwnd(win), rect);
        printf("present(rect,closed-cl) hr=0x%08X\n", (unsigned)hrRect);

        if (FAILED(hrRect)) {
            // 变体 2：Flags 签名 + open command list 重传一次
            cl->Reset(al, nullptr);
            cl->ClearRenderTargetView(rtv, red, 0, nullptr);
            auto* dlFlags = reinterpret_cast<ID3D12CommandQueueDownlevelFlags*>(dlVoid);
            HRESULT hrFlags = dlFlags->Present(cl, rt, GetHwnd(win), D3D12_DOWNLEVEL_PRESENT_FLAG_NONE);
            printf("present(flags,open-cl) hr=0x%08X\n", (unsigned)hrFlags);
        }

        usleep(2000000); // Sleep(2000) 的 POSIX 等价
        printf("presented\n");
    }
    glfwTerminate();
    printf("SPIKE DONE\n");
    return 0;
}
