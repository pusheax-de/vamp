// SpritePS.hlsl - Pixel shader for textured sprites

Texture2D textures[1024] : register(t0, space0);
SamplerState linearSampler : register(s0);

struct PSInput
{
    float4 position  : SV_POSITION;
    float2 uv        : TEXCOORD0;
    float4 color     : COLOR0;
    uint   texIndex  : TEXINDEX;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 texColor = textures[input.texIndex].Sample(linearSampler, input.uv);
    float4 outColor = texColor * input.color;
    clip(outColor.a - 0.001f);
    return outColor;
}
