// LightRadialVS.hlsl - Vertex shader for light quad rendering
// Renders a screen-space quad for each point light

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

cbuffer LightConstants : register(b1)
{
    float2 lightPosition;   // World space
    float3 lightColor;      // RGB
    float  lightRadius;     // World units
    float  lightIntensity;  // Multiplier
    float  lightPad;
};

struct VSInput
{
    uint vertexID : SV_VertexID;
};

struct VSOutput
{
    float4 position     : SV_POSITION;
    float2 worldPos     : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Generate quad corners covering the light's bounding box
    float2 corner;
    corner.x = (input.vertexID & 1) ? 1.0f : -1.0f;
    corner.y = (input.vertexID & 2) ? 1.0f : -1.0f;

    float2 worldPos = lightPosition + corner * lightRadius;
    output.position = mul(float4(worldPos, 0.0f, 1.0f), viewProjection);
    output.worldPos = worldPos;

    return output;
}
