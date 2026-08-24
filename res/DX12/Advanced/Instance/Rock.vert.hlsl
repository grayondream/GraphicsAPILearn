// 对应 res/GL/Advanced/Instance/Rock.vs：小行星带实例化。
// layout 以 SaturnApp::rockLayout 为准：binding0 交错 MeshVertex（pos@TEXCOORD0 +
// normal@TEXCOORD1 + uv@TEXCOORD2，stride sizeof(MeshVertex)）、binding3 PerInstance 的
// mat4 摊平为 TEXCOORD3..TEXCOORD6 四个 float4（PerInstance stride 64）。
// GLSL mat4 列构造/HLSL column_major 初始化器均按列填充，语义一致；
// translationMatrix[3]=…（第 4 列平移）以构造器第 4 参数表达。
// 槽位照抄：time=floatPool[10]、radiusPos=vec4Pool[45]。
#include "../../_uniform_block.hlsli"

struct VSIn {
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aTexCoords : TEXCOORD2;
    float4 aInstanceMatrix0 : TEXCOORD3;
    float4 aInstanceMatrix1 : TEXCOORD4;
    float4 aInstanceMatrix2 : TEXCOORD5;
    float4 aInstanceMatrix3 : TEXCOORD6;
};

struct VSOut {
    float4 sv : SV_Position;
    float2 TexCoords : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float radiuse = 20.0; // 半径
    o.TexCoords = i.aTexCoords;

    // 计算逆时针旋转角度
    float angle = -FPOOL(10) / 10;   // time，逆时针旋转
    float c = cos(angle);
    float s = sin(angle);

    // 创建旋转矩阵。GLSL mat4(col0,col1,col2,col3) 按列填充，而 HLSL float4x4 构造器
    // 按【行】填充——逐参数直译会得到转置矩阵（实例变换整体错乱，岩石铺满全屏）。
    // 统一 transpose() 包装使构造语义与 GLSL 逐列一致。
    float4x4 rotationMatrix = transpose(float4x4(
        float4(c, 0.0, s, 0.0),
        float4(0.0, 1.0, 0.0, 0.0),
        float4(-s, 0.0, c, 0.0),
        float4(0.0, 0.0, 0.0, 1.0)
    ));

    // 平移到中心点（单位阵 + 第 4 列 = radiusPos）
    float4x4 translationMatrix = transpose(float4x4(
        float4(1.0, 0.0, 0.0, 0.0),
        float4(0.0, 1.0, 0.0, 0.0),
        float4(0.0, 0.0, 1.0, 0.0),
        float4(gVec4Pool[45].xyz, 1.0)
    ));

    // 实例缓冲里是 glm 列主序 mat4 的四列，构造器按行吃参数 → transpose 还原
    float4x4 aInstanceMatrix = transpose(float4x4(i.aInstanceMatrix0, i.aInstanceMatrix1,
                                                  i.aInstanceMatrix2, i.aInstanceMatrix3));

    // 计算新的实例矩阵
    float4x4 newInstanceMatrix = mul(translationMatrix, mul(rotationMatrix, aInstanceMatrix));

    // 计算最终位置
    o.sv = mul(gProjection, mul(gView, mul(newInstanceMatrix, float4(i.aPos, 1.0))));
    return o;
}
