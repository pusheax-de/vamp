#pragma once
// FogRenderer.h - Fog-of-war rendering (visible/explored textures, CPU raster + GPU compose)

#include "EngineTypes.h"
#include "Texture2D.h"
#include "OccluderSet.h"
#include <d3d12.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace engine
{

// Forward declarations
class UploadManager;
class PersistentDescriptorAllocator;

// ---------------------------------------------------------------------------
// Visibility polygon vertex (for CPU LoS computation)
// ---------------------------------------------------------------------------
struct VisPolyVertex
{
    float x, y;
    float angle; // For sorting
};

// ---------------------------------------------------------------------------
// FogRenderer - CPU-side fog-of-war computation + GPU texture management
// ---------------------------------------------------------------------------
class FogRenderer
{
public:
    void Init(uint32_t fogWidth, uint32_t fogHeight,
              float worldWidth, float worldHeight,
              float worldOriginX = 0.0f, float worldOriginY = 0.0f);

    // Clear the "currently visible" mask (call at start of each vision update)
    void ClearVisible();

    // Compute and rasterize visibility from a point into the "visible now" mask
    void ComputeVisibility(float worldX, float worldY, float radius,
                            const OccluderSet& occluders);

    // Merge visible into explored (explored = max(explored, visible))
    void UpdateExplored();

    // Upload fog textures to GPU (call once per frame after all vision updates)
    void UploadToGPU(ID3D12Device* device,
                     ID3D12GraphicsCommandList* cmdList,
                     UploadManager& uploadMgr,
                     PersistentDescriptorAllocator& srvHeap);

    // Accessors for rendering
    uint32_t GetVisibleSRV() const;
    uint32_t GetExploredSRV() const;
    uint32_t GetFogWidth() const { return m_fogWidth; }
    uint32_t GetFogHeight() const { return m_fogHeight; }

    float GetWorldOriginX() const { return m_worldOriginX; }
    float GetWorldOriginY() const { return m_worldOriginY; }
    float GetWorldWidth() const { return m_worldWidth; }
    float GetWorldHeight() const { return m_worldHeight; }

    bool NeedsUpload() const { return m_dirty; }

    // Access underlying GPU resources (for creating SRVs in alternate heap locations)
    ID3D12Resource* GetVisibleResource() const { return m_ownedVisibleTex.GetResource(); }
    ID3D12Resource* GetExploredResource() const { return m_ownedExploredTex.GetResource(); }
    DXGI_FORMAT     GetTextureFormat() const { return m_ownedVisibleTex.GetFormat(); }

private:
    // World-to-fog coordinate conversion
    void WorldToFog(float wx, float wy, int& fx, int& fy) const;
    void FogToWorld(int fx, int fy, float& wx, float& wy) const;

    // CPU rasterize a triangle fan (visibility polygon) into the visible mask
    void RasterizeVisibilityPoly(const std::vector<VisPolyVertex>& poly,
                                  float centerX, float centerY);

    // Simple scanline rasterize a triangle into the mask
    void RasterizeTriangle(float x0, float y0, float x1, float y1,
                            float x2, float y2, uint8_t value);

    uint32_t    m_fogWidth      = 0;
    uint32_t    m_fogHeight     = 0;
    float       m_worldWidth    = 0.0f;
    float       m_worldHeight   = 0.0f;
    float       m_worldOriginX  = 0.0f;
    float       m_worldOriginY  = 0.0f;

    // CPU-side fog bitmaps (R8 format)
    std::vector<uint8_t> m_visibleNow;  // Cleared each frame
    std::vector<uint8_t> m_explored;    // Persistent

    // GPU textures (owned by value)
    Texture2D   m_ownedVisibleTex;
    Texture2D   m_ownedExploredTex;
    bool        m_initialized   = false;
    bool        m_dirty         = false;
};

} // namespace engine
