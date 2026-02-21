// GridOverlayVS.hlsl - Vertex shader for tile grid wireframe overlay

cbuffer OverlayConstants : register(b0)
{
    float4x4 viewProjection;
    float4   overlayColor;
};

struct VSInput
{
    float2 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 0.0f, 1.0f), viewProjection);
    return output;
}
