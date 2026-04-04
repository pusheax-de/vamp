#pragma once
// Grid.h - World/grid coordinate transforms + grid overlay rendering info
// Supports both rectangular and true isometric (sqrt(3):1 diamond) tile layouts.
// The isometric ratio matches Blender camera rotation (35.264, 0, 45).
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
    }

    // --- Isometric toggle ---
    void SetIsometric(bool iso) { m_isometric = iso; }
    bool IsIsometric() const { return m_isometric; }

    // Half-tile extents for the isometric diamond.
    // True isometric (Blender 35.264/0/45) uses width:height = sqrt(3):1,
    // so halfH = halfW / sqrt(3).
    float GetHalfW() const { return m_tileSize * 0.5f; }
    float GetHalfH() const { return m_tileSize * 0.5f / 1.7320508f; } // 0.5 / sqrt(3)

    // Tile center in world space
    DirectX::XMFLOAT2 TileToWorld(int tileX, int tileY) const
    {
        if (m_isometric)
        {
            float halfW = GetHalfW();
            float halfH = GetHalfH();
            float wx = m_originX + (m_gridHeight + tileX - tileY) * halfW;
            float wy = m_originY + (tileX + tileY) * halfH + halfH;
            return { wx, wy };
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

    // Get the 4 diamond vertices of an isometric tile in world space
    // Order: top, right, bottom, left
    void TileDiamondVertices(int tileX, int tileY,
                             DirectX::XMFLOAT2& top,
                             DirectX::XMFLOAT2& right,
                             DirectX::XMFLOAT2& bottom,
                             DirectX::XMFLOAT2& left) const
    {
        float halfW = GetHalfW();
        float halfH = GetHalfH();
        auto center = TileToWorld(tileX, tileY);
        top    = { center.x,         center.y - halfH };
        right  = { center.x + halfW, center.y         };
        bottom = { center.x,         center.y + halfH };
        left   = { center.x - halfW, center.y         };
    }

    // World position to tile coordinates (floor)
    void WorldToTile(float wx, float wy, int& tileX, int& tileY) const
    {
        if (m_isometric)
        {
            float halfW = GetHalfW();
            float halfH = GetHalfH();
            float rx = (wx - m_originX) / halfW - m_gridHeight;
            float ry = (wy - m_originY) / halfH - 1.0f;
            float ftx = (rx + ry) * 0.5f + 0.5f;
            float fty = (ry - rx) * 0.5f + 0.5f;
            tileX = static_cast<int>(std::floor(ftx));
            tileY = static_cast<int>(std::floor(fty));
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
            return (m_gridWidth + m_gridHeight) * GetHalfW();
        return m_gridWidth * m_tileSize;
    }

    float GetWorldHeight() const
    {
        if (m_isometric)
            return (m_gridWidth + m_gridHeight) * GetHalfH();
        return m_gridHeight * m_tileSize;
    }

    float GetTileSize() const { return m_tileSize; }
    int   GetGridWidth() const { return m_gridWidth; }
    int   GetGridHeight() const { return m_gridHeight; }
    float GetOriginX() const { return m_originX; }
    float GetOriginY() const { return m_originY; }

private:
    float   m_tileSize   = 32.0f;
    int     m_gridWidth  = 0;
    int     m_gridHeight = 0;
    float   m_originX    = 0.0f;
    float   m_originY    = 0.0f;
    bool    m_isometric  = false;
};

} // namespace engine
