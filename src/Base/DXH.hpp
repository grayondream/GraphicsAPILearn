#pragma once
#pragma warning(push)
#pragma warning(disable: 4005)
#pragma warning(disable: 4838)

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#include <winrt/wrl/client.h>
#include <xnamath.h>
#include <D3DX11async.h>

#if defined(DEBUG) || defined(_DEBUG)
#define DEFAULT_DX_DEVICE_FLAG D3D11_CREATE_DEVICE_DEBUG
#else
#define DEFAULT_DX_DEVICE_FLAG 0
#endif

#pragma warning(pop)