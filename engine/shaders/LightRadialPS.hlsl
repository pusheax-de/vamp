// LightRadialPS.hlsl - Pixel shader for radial point light falloff
// Output is additive-blended into LightAccum RT
// Stencil test rejects shadowed pixels

cbuffer LightConstants : register(b1)
{
    float2 lightPosition;
    float3 lightColor;
    float  lightRadius;
    float  lightIntensity;
    float  lightPad;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 worldPos : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 delta = input.worldPos - lightPosition;
    float dist = length(delta);

    // Smooth radial falloff
    float attenuation = saturate(1.0f - dist / lightRadius);
    attenuation *= attenuation; // Quadratic falloff

    float3 contribution = lightColor * lightIntensity * attenuation;
    return float4(contribution, 1.0f);
}
