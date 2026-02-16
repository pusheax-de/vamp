// FogRenderer.cpp - Fog-of-war CPU computation + GPU upload

#include "FogRenderer.h"
#include "UploadManager.h"
#include "DescriptorAllocator.h"
#include <algorithm>
#include <cmath>

namespace engine
{

void FogRenderer::Init(uint32_t fogWidth, uint32_t fogHeight,
                        float worldWidth, float worldHeight,
                        float worldOriginX, float worldOriginY)
{
    m_fogWidth    = fogWidth;
    m_fogHeight   = fogHeight;
    m_worldWidth  = worldWidth;
    m_worldHeight = worldHeight;
    m_worldOriginX = worldOriginX;
    m_worldOriginY = worldOriginY;

    m_visibleNow.resize(fogWidth * fogHeight, 0);
    m_explored.resize(fogWidth * fogHeight, 0);
    m_dirty       = true;
    m_initialized = false;
}

void FogRenderer::ClearVisible()
{
    std::memset(m_visibleNow.data(), 0, m_visibleNow.size());
    m_dirty = true;
}

void FogRenderer::ComputeVisibility(float worldX, float worldY, float radius,
                                     const OccluderSet& occluders)
{
    // Build visibility polygon using raycasting
    // Cast rays at regular angles from the observer
    constexpr int kNumRays = 180;
    std::vector<VisPolyVertex> poly;
    poly.reserve(kNumRays);

    auto segments = occluders.GetSegmentsInRadius(worldX, worldY, radius);

    for (int i = 0; i < kNumRays; ++i)
    {
        float angle = (static_cast<float>(i) / kNumRays) * 6.283185307f;
        float dirX = std::cos(angle);
        float dirY = std::sin(angle);

        // Default: ray reaches max radius
        float closest = radius;

        // Test against all nearby occluder segments
        for (const auto* seg : segments)
        {
            // Ray-segment intersection
            float ox = worldX, oy = worldY;
            float dx = dirX, dy = dirY;
            float sx = seg->a.x, sy = seg->a.y;
            float ex = seg->b.x - seg->a.x, ey = seg->b.y - seg->a.y;

            float denom = dx * ey - dy * ex;
            if (std::fabs(denom) < 1e-8f)
                continue;

            float t = ((sx - ox) * ey - (sy - oy) * ex) / denom;
            float u = ((sx - ox) * dy - (sy - oy) * dx) / denom;

            if (t > 0.0f && t < closest && u >= 0.0f && u <= 1.0f)
            {
                closest = t;
            }
        }

        VisPolyVertex v;
        v.x = worldX + dirX * closest;
        v.y = worldY + dirY * closest;
        v.angle = angle;
        poly.push_back(v);
    }

    // Sort by angle (should already be sorted, but be safe)
    std::sort(poly.begin(), poly.end(),
        [](const VisPolyVertex& a, const VisPolyVertex& b) { return a.angle < b.angle; });

    // Rasterize the visibility polygon into the fog mask
    RasterizeVisibilityPoly(poly, worldX, worldY);
    m_dirty = true;
}

void FogRenderer::UpdateExplored()
{
    for (size_t i = 0; i < m_visibleNow.size(); ++i)
    {
        if (m_visibleNow[i] > m_explored[i])
            m_explored[i] = m_visibleNow[i];
    }
}

void FogRenderer::UploadToGPU(ID3D12Device* device,
                               ID3D12GraphicsCommandList* cmdList,
                               UploadManager& uploadMgr,
                               PersistentDescriptorAllocator& srvHeap)
{
    if (!m_dirty)
        return;

    if (!m_initialized)
    {
        // Create GPU textures on first use
        // We create RGBA textures from R8 data — replicate into alpha channel
        std::vector<uint8_t> rgbaVisible(m_fogWidth * m_fogHeight * 4);
        for (uint32_t i = 0; i < m_fogWidth * m_fogHeight; ++i)
        {
            rgbaVisible[i * 4 + 0] = 255;
            rgbaVisible[i * 4 + 1] = 255;
            rgbaVisible[i * 4 + 2] = 255;
            rgbaVisible[i * 4 + 3] = m_visibleNow[i];
        }

        m_ownedVisibleTex.CreateFromRGBA(device, cmdList, uploadMgr, srvHeap,
                                          m_fogWidth, m_fogHeight, rgbaVisible.data());

        std::vector<uint8_t> rgbaExplored(m_fogWidth * m_fogHeight * 4);
        for (uint32_t i = 0; i < m_fogWidth * m_fogHeight; ++i)
        {
            rgbaExplored[i * 4 + 0] = 255;
            rgbaExplored[i * 4 + 1] = 255;
            rgbaExplored[i * 4 + 2] = 255;
            rgbaExplored[i * 4 + 3] = m_explored[i];
        }

        m_ownedExploredTex.CreateFromRGBA(device, cmdList, uploadMgr, srvHeap,
                                           m_fogWidth, m_fogHeight, rgbaExplored.data());

        m_initialized = true;
    }
    else
    {
        // Update existing textures
        std::vector<uint8_t> rgbaVisible(m_fogWidth * m_fogHeight * 4);
        for (uint32_t i = 0; i < m_fogWidth * m_fogHeight; ++i)
        {
            rgbaVisible[i * 4 + 0] = 255;
            rgbaVisible[i * 4 + 1] = 255;
            rgbaVisible[i * 4 + 2] = 255;
            rgbaVisible[i * 4 + 3] = m_visibleNow[i];
        }
        m_ownedVisibleTex.UpdateRegion(cmdList, uploadMgr, 0, 0,
                                        m_fogWidth, m_fogHeight,
                                        rgbaVisible.data(), m_fogWidth * 4);

        std::vector<uint8_t> rgbaExplored(m_fogWidth * m_fogHeight * 4);
        for (uint32_t i = 0; i < m_fogWidth * m_fogHeight; ++i)
        {
            rgbaExplored[i * 4 + 0] = 255;
            rgbaExplored[i * 4 + 1] = 255;
            rgbaExplored[i * 4 + 2] = 255;
            rgbaExplored[i * 4 + 3] = m_explored[i];
        }
        m_ownedExploredTex.UpdateRegion(cmdList, uploadMgr, 0, 0,
                                         m_fogWidth, m_fogHeight,
                                         rgbaExplored.data(), m_fogWidth * 4);
    }

    m_dirty = false;
}

uint32_t FogRenderer::GetVisibleSRV() const
{
    return m_ownedVisibleTex.IsValid() ? m_ownedVisibleTex.GetSRVIndex() : UINT32_MAX;
}

uint32_t FogRenderer::GetExploredSRV() const
{
    return m_ownedExploredTex.IsValid() ? m_ownedExploredTex.GetSRVIndex() : UINT32_MAX;
}

void FogRenderer::WorldToFog(float wx, float wy, int& fx, int& fy) const
{
    fx = static_cast<int>((wx - m_worldOriginX) / m_worldWidth * m_fogWidth);
    fy = static_cast<int>((wy - m_worldOriginY) / m_worldHeight * m_fogHeight);
}

void FogRenderer::FogToWorld(int fx, int fy, float& wx, float& wy) const
{
    wx = m_worldOriginX + (static_cast<float>(fx) / m_fogWidth) * m_worldWidth;
    wy = m_worldOriginY + (static_cast<float>(fy) / m_fogHeight) * m_worldHeight;
}

void FogRenderer::RasterizeVisibilityPoly(const std::vector<VisPolyVertex>& poly,
                                           float centerX, float centerY)
{
    if (poly.size() < 2)
        return;

    // Rasterize as triangle fan: center + consecutive pairs of polygon vertices
    for (size_t i = 0; i < poly.size(); ++i)
    {
        size_t next = (i + 1) % poly.size();
        RasterizeTriangle(centerX, centerY,
                           poly[i].x, poly[i].y,
                           poly[next].x, poly[next].y,
                           255);
    }
}

void FogRenderer::RasterizeTriangle(float x0, float y0,
                                     float x1, float y1,
                                     float x2, float y2,
                                     uint8_t value)
{
    // Convert to fog coordinates
    int fx0, fy0, fx1, fy1, fx2, fy2;
    WorldToFog(x0, y0, fx0, fy0);
    WorldToFog(x1, y1, fx1, fy1);
    WorldToFog(x2, y2, fx2, fy2);

    // Bounding box
    int minX = (std::min)({ fx0, fx1, fx2 });
    int maxX = (std::max)({ fx0, fx1, fx2 });
    int minY = (std::min)({ fy0, fy1, fy2 });
    int maxY = (std::max)({ fy0, fy1, fy2 });

    minX = (std::max)(0, minX);
    maxX = (std::min)(static_cast<int>(m_fogWidth) - 1, maxX);
    minY = (std::max)(0, minY);
    maxY = (std::min)(static_cast<int>(m_fogHeight) - 1, maxY);

    // Barycentric rasterization
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            // Edge functions
            float w0 = static_cast<float>((fx1 - fx0) * (y - fy0) - (fy1 - fy0) * (x - fx0));
            float w1 = static_cast<float>((fx2 - fx1) * (y - fy1) - (fy2 - fy1) * (x - fx1));
            float w2 = static_cast<float>((fx0 - fx2) * (y - fy2) - (fy0 - fy2) * (x - fx2));

            if ((w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) ||
                (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f))
            {
                m_visibleNow[y * m_fogWidth + x] = value;
            }
        }
    }
}

} // namespace engine
