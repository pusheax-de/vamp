// GridOverlayVS.hlsl - Vertex shader for tile grid wireframe overlay

cbuffer FrameConstants : register(b0)
{
    float4x4 viewProjection;
    float2   cameraPosition;
    float    cameraZoom;
    float    time;
    float2   screenSize;
    float2   fogTexelSize;
    float    ambientDarkening;
    float3   pad;
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
