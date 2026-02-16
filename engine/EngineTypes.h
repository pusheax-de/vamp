#pragma once
// EngineTypes.h - Common types and forward declarations for the rendering engine

#include <cstdint>
#include <DirectXMath.h>

namespace engine
{

// ---------------------------------------------------------------------------
// Handle types (lightweight indices into internal arrays)
// ---------------------------------------------------------------------------
struct TextureHandle
{
    uint32_t index = UINT32_MAX;
    bool IsValid() const { return index != UINT32_MAX; }
};

struct RenderTargetHandle
{
    uint32_t index = UINT32_MAX;
    bool IsValid() const { return index != UINT32_MAX; }
};

// ---------------------------------------------------------------------------
// Render layers (draw order)
// ---------------------------------------------------------------------------
enum class RenderLayer : uint8_t
{
    BackgroundPages = 0,
    GroundTiles,        // Decals, blood, scorch marks
    WallsProps,         // Walls rendered as separate sprites
    Actors,             // Characters, NPCs (Y-sorted)
    Roofs,              // Building roofs (conditionally visible)
    RoofActors,         // Characters on top of roofs
    ScreenSpaceIcons,   // Health bars, selection circles, quest markers
    FogOverlay,         // Fog-of-war darkening
    COUNT
};

// ---------------------------------------------------------------------------
// Vertex for 2D sprite/quad rendering
// ---------------------------------------------------------------------------
struct SpriteVertex
{
    DirectX::XMFLOAT2 position;     // World-space position
    DirectX::XMFLOAT2 uv;           // Texture coordinates
    DirectX::XMFLOAT4 color;        // Tint / alpha
};

// ---------------------------------------------------------------------------
// Per-instance data for instanced sprite drawing
// ---------------------------------------------------------------------------
struct SpriteInstance
{
    DirectX::XMFLOAT2 position;     // World-space center
    DirectX::XMFLOAT2 size;         // Width, Height in world units
    DirectX::XMFLOAT4 uvRect;       // (u0, v0, u1, v1) in atlas
    DirectX::XMFLOAT4 color;        // Tint / alpha
    float              rotation;     // Radians
    float              sortY;        // For Y-sorting within a layer
    uint32_t           textureIndex; // Index into descriptor table
    uint32_t           pad;
};

// ---------------------------------------------------------------------------
// Sort key for render queue
// ---------------------------------------------------------------------------
struct RenderSortKey
{
    uint64_t key;

    static RenderSortKey Make(RenderLayer layer, float ySort, uint16_t priority,
                               uint16_t materialId)
    {
        RenderSortKey k;
        // Layout: [layer:8][ySort:24][priority:16][material:16]
        uint64_t l = static_cast<uint64_t>(layer) & 0xFF;
        // Convert float y to a sortable integer (offset to make positive, then scale)
        int32_t yInt = static_cast<int32_t>((ySort + 32768.0f) * 256.0f);
        uint64_t y = static_cast<uint64_t>(yInt & 0x00FFFFFF);
        uint64_t p = static_cast<uint64_t>(priority) & 0xFFFF;
        uint64_t m = static_cast<uint64_t>(materialId) & 0xFFFF;
        k.key = (l << 56) | (y << 32) | (p << 16) | m;
        return k;
    }

    bool operator<(const RenderSortKey& other) const { return key < other.key; }
};

// ---------------------------------------------------------------------------
// Light descriptor
// ---------------------------------------------------------------------------
struct PointLight2D
{
    DirectX::XMFLOAT2 position;     // World space
    DirectX::XMFLOAT3 color;        // RGB, HDR-capable
    float              radius;       // Falloff radius in world units
    float              intensity;    // Multiplier
    float              flickerPhase; // For animated flicker
};

// ---------------------------------------------------------------------------
// Occluder segment (wall edge for LoS / shadows)
// ---------------------------------------------------------------------------
struct OccluderSegment
{
    DirectX::XMFLOAT2 a;            // Start point (world space)
    DirectX::XMFLOAT2 b;            // End point (world space)
    bool               isCover;      // Also provides gameplay cover
};

// ---------------------------------------------------------------------------
// Roof volume
// ---------------------------------------------------------------------------
struct RoofFootprint
{
    // Axis-aligned bounding box in tile coordinates
    int x0, y0, x1, y1;
    TextureHandle roofTexture;
    float         currentAlpha;      // For fade animation
    bool          isInterior;        // True if player is inside
};

// ---------------------------------------------------------------------------
// Background page info
// ---------------------------------------------------------------------------
struct BackgroundPage
{
    int             pageX, pageY;    // Page grid coordinates
    TextureHandle   texture;
    bool            isLoaded;
    uint32_t        lastUsedFrame;   // For LRU eviction
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint32_t kMaxFramesInFlight    = 2;
static constexpr uint32_t kMaxLights            = 32;
static constexpr uint32_t kMaxBackgroundPages   = 256;
static constexpr uint32_t kMaxSpriteInstances   = 4096;
static constexpr uint32_t kPageSizePixels       = 512;

static constexpr uint32_t kMaxPersistentSRVs    = 1024;
static constexpr uint32_t kMaxPerFrameSRVs      = 512;

} // namespace engine
