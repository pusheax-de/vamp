// SpritePointPS.hlsl - Pixel shader for sprites using point (nearest) sampling
// Used for UI text and pixel-perfect screen-space elements.

Texture2D textures[1024] : register(t0, space0);
SamplerState pointSampler : register(s1);

struct PSInput
{
    float4 position  : SV_POSITION;
    float2 uv        : TEXCOORD0;
    float4 color     : COLOR0;
    uint   texIndex  : TEXINDEX;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 texColor = textures[input.texIndex].Sample(pointSampler, input.uv);
    return texColor * input.color;
}
