// CompositePS.hlsl - Final compositing: SceneColor * fog + LightAccum
// Combines base scene with dynamic lighting and fog-of-war

cbuffer FrameConstants : register(b0)
{
    float4x4 viewProjection;
    float2   cameraPosition;
    float    cameraZoom;
    float    time;
    float2   screenSize;
    float2   fogTexelSize;
    float    ambientDarkening;  // Base brightness in full darkness (0.0-1.0)
    float3   fogColor;          // RGB color for unexplored fog areas
    float2   fogWorldOrigin;    // World-space origin of fog texture
    float2   fogWorldSize;      // World-space extent of fog texture
};

Texture2D sceneColorTex : register(t0);
Texture2D lightAccumTex : register(t1);
Texture2D fogVisibleTex : register(t2);
Texture2D fogExploredTex : register(t3);

SamplerState pointSampler  : register(s0);
SamplerState linearSampler : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 sceneColor = sceneColorTex.Sample(pointSampler, input.uv).rgb;
    float3 lightAccum = lightAccumTex.Sample(linearSampler, input.uv).rgb;

    // Convert screen UV to world position, then to fog UV
    float2 screenPos = input.uv * screenSize;
    float2 worldPos;
    worldPos.x = (screenPos.x - screenSize.x * 0.5f) / cameraZoom + cameraPosition.x;
    worldPos.y = (screenPos.y - screenSize.y * 0.5f) / cameraZoom + cameraPosition.y;
    float2 fogUV = (worldPos - fogWorldOrigin) / fogWorldSize;

    // Sample fog textures (alpha channel holds visibility)
    float visible  = fogVisibleTex.Sample(linearSampler, fogUV).a;
    float explored = fogExploredTex.Sample(linearSampler, fogUV).a;

    // Fog darkening factor:
    //   visible:                   fogFactor = 1.0 (full brightness)
    //   explored but not visible:  fogFactor = 0.4 (dim memory)
    //   not explored:              fogFactor = ambientDarkening (near black)
    float fogFactor = ambientDarkening;
    fogFactor = lerp(fogFactor, 0.4f, explored);
    fogFactor = lerp(fogFactor, 1.0f, visible);

    // Compose: lerp scene toward fog color based on darkening, then add dynamic lights
    float3 finalColor = lerp(fogColor, sceneColor, fogFactor) + lightAccum * visible;

    return float4(finalColor, 1.0f);
}
