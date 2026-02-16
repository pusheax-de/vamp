// CompositeVS.hlsl - Fullscreen triangle vertex shader
// Generates a single triangle that covers the entire screen

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    // Fullscreen triangle: vertices at (-1,-1), (3,-1), (-1,3)
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    output.uv.y = 1.0f - output.uv.y; // Flip Y for texture sampling

    return output;
}
