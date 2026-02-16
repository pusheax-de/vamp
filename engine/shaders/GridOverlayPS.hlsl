// GridOverlayPS.hlsl - Pixel shader for tile grid wireframe overlay

struct PSInput
{
    float4 position : SV_POSITION;
};

float4 main(PSInput input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 0.15f); // Semi-transparent white lines
}
