#pragma once

enum class GraphicsType : int{
    GL,
    DX11,
    DX12,
    Metal,
    Vulkan,
    WebGPU
};

enum class AppType : int{
    Base,
    Triangle,
    Rect,
    SimpleTexture,
    Cube,
    Camera,
    SimpleLight_Ambination,
    SimpleLight_Diffuse,
    SimpleLight_Specular,
    SimpleLight_Material,
    SimpleLight_Map,
};

