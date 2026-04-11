#pragma once
// Grid.h - World/grid coordinate transforms + grid overlay rendering info
// Supports both rectangular and isometric hexagonal tile layouts.
// Isometric mode stores a flat-top hex grid in logical tile space and then
// projects it into screen/world space using a classic 2:1 isometric transform.
// This matches the visual footprint expected from Fallout-style maps instead
// of drawing a top-down hex that has only been vertically squashed.
//
// In both modes, the origin (m_originX, m_originY) represents the top-left
// corner of the grid's world-space bounding box. GetWorldWidth/Height give
// the total extent from that corner.

#include "EngineTypes.h"
#include <DirectXMath.h>
#include <cmath>

namespace engine
{

class Grid
{
public:
    void Init(float tileSizePixels, int gridWidth, int gridHeight,
              float originX = 0.0f, float originY = 0.0f)
    {
        m_tileSize   = tileSizePixels;
        m_gridWidth  = gridWidth;
        m_gridHeight = gridHeight;
        m_originX    = originX;
        m_originY    = originY;
        UpdateDerivedMetrics();
    }

    // --- Isometric toggle ---
    void SetIsometric(bool iso)
    {
        m_isometric = iso;
        UpdateDerivedMetrics();
    }
    bool IsIsometric() const { return m_isometric; }

    // Half extents of the tile's projected screen-space bounding box.
    float GetHalfW() const
    {
        return m_isometric ? m_isoHexHalfW : (m_tileSize * 0.5f);
    }
    float GetHalfH() const
    {
        return m_isometric ? m_isoHexHalfH : (m_tileSize * 0.5f);
    }

    // Tile center in world space.
    DirectX::XMFLOAT2 TileToWorld(int tileX, int tileY) const
    {
        if (m_isometric)
        {
            const auto logical = TileToLogicalHexCenter(tileX, tileY);
            const auto projected = ProjectIsometric(logical.x, logical.y);
            return { m_originX + (projected.x - m_isoBoundsMinX),
                     m_originY + (projected.y - m_isoBoundsMinY) };
        }
        else
        {
            float wx = m_originX + (tileX + 0.5f) * m_tileSize;
            float wy = m_originY + (tileY + 0.5f) * m_tileSize;
            return { wx, wy };
        }
    }

    // Tile top-left corner in world space (rectangular only; for iso returns center)
    DirectX::XMFLOAT2 TileTopLeft(int tileX, int tileY) const
    {
        if (m_isometric)
            return TileToWorld(tileX, tileY);

        float wx = m_originX + tileX * m_tileSize;
        float wy = m_originY + tileY * m_tileSize;
        return { wx, wy };
    }

    // Get the 6 hex vertices of an isometric tile in world space.
    // The vertex order is the logical hex order before projection and remains
    // clockwise after projection because the isometric transform is linear.
    void TileHexVertices(int tileX, int tileY,
                         DirectX::XMFLOAT2 outVerts[6]) const
    {
        const auto center = TileToWorld(tileX, tileY);
        const float radius = GetLogicalHexRadius();
        const float logicalHalfH = radius * kSqrt3 * 0.5f;
        const float localVerts[6][2] = {
            { -radius,          0.0f         },
            { -radius * 0.5f,  -logicalHalfH },
            {  radius * 0.5f,  -logicalHalfH },
            {  radius,          0.0f         },
            {  radius * 0.5f,   logicalHalfH },
            { -radius * 0.5f,   logicalHalfH },
        };

        for (int i = 0; i < 6; ++i)
        {
            const auto projected = ProjectIsometric(localVerts[i][0], localVerts[i][1]);
            outVerts[i] = { center.x + projected.x, center.y + projected.y };
        }
    }

    // Legacy 4-vertex bounding diamond (uses hex bounding box)
    void TileDiamondVertices(int tileX, int tileY,
                             DirectX::XMFLOAT2& top,
                             DirectX::XMFLOAT2& right,
                             DirectX::XMFLOAT2& bottom,
                             DirectX::XMFLOAT2& left) const
    {
        if (!m_isometric)
        {
            float halfW = GetHalfW();
            float halfH = GetHalfH();
            auto center = TileToWorld(tileX, tileY);
            top    = { center.x,         center.y - halfH };
            right  = { center.x + halfW, center.y         };
            bottom = { center.x,         center.y + halfH };
            left   = { center.x - halfW, center.y         };
            return;
        }

        DirectX::XMFLOAT2 hex[6];
        TileHexVertices(tileX, tileY, hex);

        top = bottom = left = right = hex[0];
        for (int i = 1; i < 6; ++i)
        {
            if (hex[i].y < top.y) top = hex[i];
            if (hex[i].y > bottom.y) bottom = hex[i];
            if (hex[i].x < left.x) left = hex[i];
            if (hex[i].x > right.x) right = hex[i];
        }
    }

    // Hex neighbor directions (flat-top, offset columns, odd-shift-down).
    // Index 0..5 corresponds to the logical hex edges before projection.
    struct TileCoord { int x, y; };

    static void HexNeighbor(int tileX, int tileY, int edgeIndex, int& nx, int& ny)
    {
        static const int evenOffsets[6][2] = {
            { -1, -1 },
            {  0, -1 },
            { +1, -1 },
            { +1,  0 },
            {  0, +1 },
            { -1,  0 },
        };
        static const int oddOffsets[6][2] = {
            { -1,  0 },
            {  0, -1 },
            { +1,  0 },
            { +1, +1 },
            {  0, +1 },
            { -1, +1 },
        };
        const auto& off = (tileX & 1) ? oddOffsets[edgeIndex] : evenOffsets[edgeIndex];
        nx = tileX + off[0];
        ny = tileY + off[1];
    }

    // World position to tile coordinates (floor)
    void WorldToTile(float wx, float wy, int& tileX, int& tileY) const
    {
        if (m_isometric)
        {
            float bestDist = 1e30f;
            int bestX = 0;
            int bestY = 0;

            for (int y = 0; y < m_gridHeight; ++y)
            {
                for (int x = 0; x < m_gridWidth; ++x)
                {
                    if (PointInHex(wx, wy, x, y))
                    {
                        tileX = x;
                        tileY = y;
                        return;
                    }

                    const auto center = TileToWorld(x, y);
                    const float dx = wx - center.x;
                    const float dy = wy - center.y;
                    const float dist = dx * dx + dy * dy;
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestX = x;
                        bestY = y;
                    }
                }
            }

            tileX = bestX;
            tileY = bestY;
        }
        else
        {
            tileX = static_cast<int>(std::floor((wx - m_originX) / m_tileSize));
            tileY = static_cast<int>(std::floor((wy - m_originY) / m_tileSize));
        }
    }

    // Check if tile coords are valid
    bool InBounds(int tileX, int tileY) const
    {
        return tileX >= 0 && tileX < m_gridWidth &&
               tileY >= 0 && tileY < m_gridHeight;
    }

    // World-space bounding box extent of the entire grid
    float GetWorldWidth() const
    {
        if (m_isometric)
            return m_isoWorldWidth;
        return m_gridWidth * m_tileSize;
    }

    float GetWorldHeight() const
    {
        if (m_isometric)
            return m_isoWorldHeight;
        return m_gridHeight * m_tileSize;
    }

    float GetTileSize() const { return m_tileSize; }
    int   GetGridWidth() const { return m_gridWidth; }
    int   GetGridHeight() const { return m_gridHeight; }
    float GetOriginX() const { return m_originX; }
    float GetOriginY() const { return m_originY; }

private:
    static constexpr float kSqrt3 = 1.7320508f;

    float GetLogicalHexRadius() const
    {
        // Choose the logical hex radius so the projected width remains equal to
        // tileSize and the projected height becomes tileSize / 2.
        return m_tileSize / (1.0f + kSqrt3);
    }

    DirectX::XMFLOAT2 TileToLogicalHexCenter(int tileX, int tileY) const
    {
        const float radius = GetLogicalHexRadius();
        const float logicalX = 1.5f * radius * static_cast<float>(tileX);
        const float logicalY = kSqrt3 * radius
            * (static_cast<float>(tileY) + ((tileX & 1) ? 0.5f : 0.0f));
        return { logicalX, logicalY };
    }

    static DirectX::XMFLOAT2 ProjectIsometric(float x, float y)
    {
        return { x - y, (x + y) * 0.5f };
    }

    bool PointInHex(float wx, float wy, int tileX, int tileY) const
    {
        DirectX::XMFLOAT2 verts[6];
        TileHexVertices(tileX, tileY, verts);

        bool hasNegative = false;
        bool hasPositive = false;

        for (int i = 0; i < 6; ++i)
        {
            const DirectX::XMFLOAT2& a = verts[i];
            const DirectX::XMFLOAT2& b = verts[(i + 1) % 6];
            const float cross = (b.x - a.x) * (wy - a.y) - (b.y - a.y) * (wx - a.x);
            if (cross < -0.001f) hasNegative = true;
            if (cross >  0.001f) hasPositive = true;
            if (hasNegative && hasPositive)
                return false;
        }

        return true;
    }

    void UpdateDerivedMetrics()
    {
        if (!m_isometric || m_gridWidth <= 0 || m_gridHeight <= 0)
        {
            m_isoBoundsMinX = 0.0f;
            m_isoBoundsMinY = 0.0f;
            m_isoBoundsMaxX = 0.0f;
            m_isoBoundsMaxY = 0.0f;
            m_isoWorldWidth = 0.0f;
            m_isoWorldHeight = 0.0f;
            m_isoHexHalfW = m_tileSize * 0.5f;
            m_isoHexHalfH = m_tileSize * 0.5f;
            return;
        }

        const float radius = GetLogicalHexRadius();
        const float logicalHalfH = radius * kSqrt3 * 0.5f;
        const float localVerts[6][2] = {
            { -radius,          0.0f         },
            { -radius * 0.5f,  -logicalHalfH },
            {  radius * 0.5f,  -logicalHalfH },
            {  radius,          0.0f         },
            {  radius * 0.5f,   logicalHalfH },
            { -radius * 0.5f,   logicalHalfH },
        };

        float localMinX = 1e30f;
        float localMinY = 1e30f;
        float localMaxX = -1e30f;
        float localMaxY = -1e30f;
        for (int i = 0; i < 6; ++i)
        {
            const auto projected = ProjectIsometric(localVerts[i][0], localVerts[i][1]);
            if (projected.x < localMinX) localMinX = projected.x;
            if (projected.y < localMinY) localMinY = projected.y;
            if (projected.x > localMaxX) localMaxX = projected.x;
            if (projected.y > localMaxY) localMaxY = projected.y;
        }
        m_isoHexHalfW = (localMaxX - localMinX) * 0.5f;
        m_isoHexHalfH = (localMaxY - localMinY) * 0.5f;

        m_isoBoundsMinX = 1e30f;
        m_isoBoundsMinY = 1e30f;
        m_isoBoundsMaxX = -1e30f;
        m_isoBoundsMaxY = -1e30f;

        for (int y = 0; y < m_gridHeight; ++y)
        {
            for (int x = 0; x < m_gridWidth; ++x)
            {
                const auto logicalCenter = TileToLogicalHexCenter(x, y);
                const auto center = ProjectIsometric(logicalCenter.x, logicalCenter.y);
                for (int i = 0; i < 6; ++i)
                {
                    const auto projected = ProjectIsometric(localVerts[i][0], localVerts[i][1]);
                    const float vx = center.x + projected.x;
                    const float vy = center.y + projected.y;
                    if (vx < m_isoBoundsMinX) m_isoBoundsMinX = vx;
                    if (vy < m_isoBoundsMinY) m_isoBoundsMinY = vy;
                    if (vx > m_isoBoundsMaxX) m_isoBoundsMaxX = vx;
                    if (vy > m_isoBoundsMaxY) m_isoBoundsMaxY = vy;
                }
            }
        }

        m_isoWorldWidth = m_isoBoundsMaxX - m_isoBoundsMinX;
        m_isoWorldHeight = m_isoBoundsMaxY - m_isoBoundsMinY;
    }

    float   m_tileSize   = 32.0f;
    int     m_gridWidth  = 0;
    int     m_gridHeight = 0;
    float   m_originX    = 0.0f;
    float   m_originY    = 0.0f;
    bool    m_isometric  = false;
    float   m_isoBoundsMinX = 0.0f;
    float   m_isoBoundsMinY = 0.0f;
    float   m_isoBoundsMaxX = 0.0f;
    float   m_isoBoundsMaxY = 0.0f;
    float   m_isoWorldWidth = 0.0f;
    float   m_isoWorldHeight = 0.0f;
    float   m_isoHexHalfW = 16.0f;
    float   m_isoHexHalfH = 8.0f;
};

} // namespace engine
