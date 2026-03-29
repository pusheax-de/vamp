#pragma once
// Grid.h - World/grid coordinate transforms + grid overlay rendering info
// Supports both rectangular and isometric (2:1 diamond) tile layouts.
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

    // Tile center in world space
    DirectX::XMFLOAT2 TileToWorld(int tileX, int tileY) const
    {
        if (m_isometric)
        {
            // In isometric mode the bounding box top-left is at (m_originX, m_originY).
            //
            // The diamond grid is laid out so that:
            //   - Tile (0,0) top vertex is at the top-center of the bbox.
            //   - Increasing tileX moves right-and-down.
            //   - Increasing tileY moves left-and-down.
            //
            // halfW = tileSize/2  (half diamond width)
            // halfH = tileSize/4  (half diamond height)
            //
            // Tile center relative to bbox top-left:
            //   cx = gridHeight * halfW  + (tileX - tileY) * halfW
            //   cy =                       (tileX + tileY) * halfH  + halfH
            //
            // The first term for cx offsets so tile(0,0) lands at the top-center.
            // The +halfH in cy places the center one halfH below the top vertex.

            float halfW = m_tileSize * 0.5f;
            float halfH = m_tileSize * 0.25f;
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
        float halfW = m_tileSize * 0.5f;
        float halfH = m_tileSize * 0.25f;
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
            float halfW = m_tileSize * 0.5f;
            float halfH = m_tileSize * 0.25f;
            // Invert TileToWorld:
            //   wx = originX + (gridH + tx - ty) * halfW
            //   wy = originY + (tx + ty + 1) * halfH          (the +1 is the +halfH offset)
            //
            // Let rx = (wx - originX) / halfW - gridH          = tx - ty
            //     ry = (wy - originY) / halfH - 1              = tx + ty
            //
            //   ftx = (rx + ry) / 2  = tx      (exact at tile center)
            //   fty = (ry - rx) / 2  = ty      (exact at tile center)
            //
            // The 4 diamond vertices of tile (tx,ty) map to a unit square
            // centred on (tx, ty) in (ftx, fty) space, spanning
            // [tx-0.5, tx+0.5) x [ty-0.5, ty+0.5).  Adding 0.5 before
            // floor() shifts this to [tx, tx+1) so floor gives tx.
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
            return (m_gridWidth + m_gridHeight) * m_tileSize * 0.5f;
        return m_gridWidth * m_tileSize;
    }

    float GetWorldHeight() const
    {
        if (m_isometric)
            return (m_gridWidth + m_gridHeight) * m_tileSize * 0.25f;
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
