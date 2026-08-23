#pragma once
// WIN32_LEAN_AND_MEAN/NOMINMAX 必须在 windows.h 前：min/max 等宏会破坏同 TU 后续
// 头文件（imgui/std）的函数调用，且 dx12 头不需要那些遗留组件。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>   // ID3D12Debug/ID3D12InfoQueue（调试层诊断）
#include <dxgi1_6.h>
#include <dxcapi.h>
#include "base/Log.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#define DX_CHECK(hr, msg) \
    do { HRESULT _hr = (hr); if (FAILED(_hr)) { \
        LOGE("[DX12] {} failed hr=0x{:08X}", msg, (uint32_t)_hr); \
        dxdiag::DumpMessages(msg); } } while (0)

namespace rhi {
namespace dxdiag {
// 设备侧诊断：DXRenderer::init 注入 InfoQueue（可选调试层，见 DXBackend.cpp），
// DX_CHECK 失败时把设备校验消息带出到应用日志（E_INVALIDARG 类问题的定位基石）
void SetInfoQueue(ID3D12Device* device);
void DumpMessages(const char* context);
} // namespace dxdiag

template <typename T>
struct ComPtr {
    T* ptr{nullptr};
    ComPtr() = default;
    ~ComPtr() { if (ptr) ptr->Release(); }
    // 禁拷贝防双重 Release；移动后源指针置空
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        // 不能写 &other：operator& 被重载为返回 T**，须用 addressof 取对象地址
        if (this != std::addressof(other)) {
            if (ptr) ptr->Release();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    T** operator&() { return &ptr; }
    T* operator->() const { return ptr; }
    T* Get() const { return ptr; }
};
using namespace std::string_literals;
} // namespace rhi
