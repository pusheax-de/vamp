// SpriteHardCutoutPS.hlsl - Point-sampled binary cutout for solid objects

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
    float4 outColor = texColor * input.color;
    clip(outColor.a - 0.5f);
    return float4(outColor.rgb, 1.0f);
}
