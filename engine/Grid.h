#pragma once
// Grid.h - World/grid coordinate transforms + grid overlay rendering info

#include "EngineTypes.h"
#include <DirectXMath.h>

namespace engine
{

// ---------------------------------------------------------------------------
// Grid - maps between world-space pixels and tile coordinates
// ---------------------------------------------------------------------------
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

    // Tile center in world space
    DirectX::XMFLOAT2 TileToWorld(int tileX, int tileY) const
    {
        float wx = m_originX + (tileX + 0.5f) * m_tileSize;
        float wy = m_originY + (tileY + 0.5f) * m_tileSize;
        return { wx, wy };
    }

    // Tile top-left corner in world space
    DirectX::XMFLOAT2 TileTopLeft(int tileX, int tileY) const
    {
        float wx = m_originX + tileX * m_tileSize;
        float wy = m_originY + tileY * m_tileSize;
        return { wx, wy };
    }

    // World position to tile coordinates (floor)
    void WorldToTile(float wx, float wy, int& tileX, int& tileY) const
    {
        tileX = static_cast<int>((wx - m_originX) / m_tileSize);
        tileY = static_cast<int>((wy - m_originY) / m_tileSize);
    }

    // Check if tile coords are valid
    bool InBounds(int tileX, int tileY) const
    {
        return tileX >= 0 && tileX < m_gridWidth &&
               tileY >= 0 && tileY < m_gridHeight;
    }

    // World-space extent of the entire grid
    float GetWorldWidth() const { return m_gridWidth * m_tileSize; }
    float GetWorldHeight() const { return m_gridHeight * m_tileSize; }

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
};

} // namespace engine
