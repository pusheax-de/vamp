// SpriteVS.hlsl - Vertex shader for instanced 2D sprite rendering

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
    // Per-vertex (quad corner)
    uint vertexID : SV_VertexID;

    // Per-instance
    float2 instancePos      : INST_POS;
    float2 instanceSize     : INST_SIZE;
    float4 instanceUVRect   : INST_UVRECT;  // (u0, v0, u1, v1)
    float4 instanceColor    : INST_COLOR;
    float  instanceRotation : INST_ROT;
    float  instanceSortY    : INST_SORTY;
    uint   instanceTexIndex : INST_TEX;
    uint   instancePad      : INST_PAD;
};

struct VSOutput
{
    float4 position  : SV_POSITION;
    float2 uv        : TEXCOORD0;
    float4 color     : COLOR0;
    uint   texIndex  : TEXINDEX;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Generate quad corners from vertex ID (0-3 ? 4 corners, rendered as triangle strip)
    // 0=TL, 1=TR, 2=BL, 3=BR
    float2 corner;
    corner.x = (input.vertexID & 1) ? 0.5f : -0.5f;
    corner.y = (input.vertexID & 2) ? 0.5f : -0.5f;

    // Apply rotation
    float cs = cos(input.instanceRotation);
    float sn = sin(input.instanceRotation);
    float2 rotated;
    rotated.x = corner.x * cs - corner.y * sn;
    rotated.y = corner.x * sn + corner.y * cs;

    // Scale and translate
    float2 worldPos = input.instancePos + rotated * input.instanceSize;

    output.position = mul(float4(worldPos, 0.0f, 1.0f), viewProjection);

    // UV interpolation
    float2 uvCorner;
    uvCorner.x = (input.vertexID & 1) ? input.instanceUVRect.z : input.instanceUVRect.x;
    uvCorner.y = (input.vertexID & 2) ? input.instanceUVRect.w : input.instanceUVRect.y;
    output.uv = uvCorner;

    output.color    = input.instanceColor;
    output.texIndex = input.instanceTexIndex;

    return output;
}
