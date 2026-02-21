// ShadowVolumeVS.hlsl - Vertex shader for 2D shadow volume quads
// Color writes are OFF — this only writes to stencil

cbuffer FrameConstants : register(b0)
{
    float4x4 viewProjection;
    float2   cameraPosition;
    float    cameraZoom;
    float    time;
    float2   screenSize;
    float2   fogTexelSize;
    float    ambientDarkening;
    float3   fogColor;
    float2   fogWorldOrigin;
    float2   fogWorldSize;
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
