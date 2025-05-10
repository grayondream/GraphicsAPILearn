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
    SimpleLight_Source_Direction,
    SimpleLight_Source_Point,
    SimpleLight_Source_Spot,
    SimpleLight_Source_Mult,
    LoadModel,
    DepthTest,
    TemplateTest,
    Blend,
    CullFace,
    FrameBuffer,
    SkyBox,
    AdvancedGLSL,
    UniformBuffer,
    SimpleGeometry,
    Explode,
    NormalLine,
    MultiInstance
};

