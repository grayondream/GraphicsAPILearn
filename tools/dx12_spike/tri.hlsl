cbuffer C : register(b0) { float4 x; };
struct VOut { float4 p : SV_Position; };
VOut VSMain(uint v : SV_VertexID) {
    float2 pts[3] = { {-1,-1}, {3,-1}, {-1,3} };
    VOut o; o.p = float4(pts[v], 0, 1); return o;
}
[shader("geometry")]
[maxvertexcount(3)]
void GSMain(triangle VOut inp[3], inout TriangleStream<VOut> s) { s.Append(inp[0]); s.Append(inp[1]); s.Append(inp[2]); s.RestartStrip(); }
float4 PSMain(VOut i) : SV_Target { return float4(0,1,0,1); }
