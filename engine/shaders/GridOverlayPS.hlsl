// GridOverlayPS.hlsl - Pixel shader for tile grid wireframe overlay

cbuffer OverlayConstants : register(b0)
{
    float4x4 viewProjection;
    float4   overlayColor;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

float4 main(PSInput input) : SV_TARGET
{
    return overlayColor;
}
