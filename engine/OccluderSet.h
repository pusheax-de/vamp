#pragma once
// OccluderSet.h - 2D occluder segments for walls (LoS + shadow casting)

#include "EngineTypes.h"
#include "Grid.h"
#include <vector>
#include <DirectXMath.h>
#include <cmath>

namespace engine
{

class Grid;

// ---------------------------------------------------------------------------
// OccluderSet - collection of wall/cover segments for a scene
// ---------------------------------------------------------------------------
class OccluderSet
{
public:
    void Clear() { m_segments.clear(); }

    void AddSegment(float ax, float ay, float bx, float by, bool isCover = false)
    {
        OccluderSegment seg;
        seg.a = { ax, ay };
        seg.b = { bx, by };
        seg.isCover = isCover;
        m_segments.push_back(seg);
    }

    // Build occluders from a tile grid (extract wall edges) — rectangular mode
    void BuildFromTileGrid(const bool* blocksLoS, int gridWidth, int gridHeight,
                            float tileSize, float originX, float originY)
    {
        m_segments.clear();

        for (int y = 0; y < gridHeight; ++y)
        {
            for (int x = 0; x < gridWidth; ++x)
            {
                if (!blocksLoS[y * gridWidth + x])
                    continue;

                float wx = originX + x * tileSize;
                float wy = originY + y * tileSize;

                // North edge (if neighbor to north is open)
                if (y == 0 || !blocksLoS[(y - 1) * gridWidth + x])
                    AddSegment(wx, wy, wx + tileSize, wy);

                // South edge
                if (y == gridHeight - 1 || !blocksLoS[(y + 1) * gridWidth + x])
                    AddSegment(wx, wy + tileSize, wx + tileSize, wy + tileSize);

                // West edge
                if (x == 0 || !blocksLoS[y * gridWidth + (x - 1)])
                    AddSegment(wx, wy, wx, wy + tileSize);

                // East edge
                if (x == gridWidth - 1 || !blocksLoS[y * gridWidth + (x + 1)])
                    AddSegment(wx + tileSize, wy, wx + tileSize, wy + tileSize);
            }
        }
    }

    // Build occluders from a tile grid — isometric mode (diamond edges)
    void BuildFromTileGridIsometric(const bool* blocksLoS, int gridWidth, int gridHeight,
                                     const Grid& grid)
    {
        m_segments.clear();

        for (int y = 0; y < gridHeight; ++y)
        {
            for (int x = 0; x < gridWidth; ++x)
            {
                if (!blocksLoS[y * gridWidth + x])
                    continue;

                DirectX::XMFLOAT2 top, right, bottom, left;
                grid.TileDiamondVertices(x, y, top, right, bottom, left);

                // North edge (top?right): neighbor y-1
                if (y == 0 || !blocksLoS[(y - 1) * gridWidth + x])
                    AddSegment(top.x, top.y, right.x, right.y);

                // East edge (right?bottom): neighbor x+1
                if (x == gridWidth - 1 || !blocksLoS[y * gridWidth + (x + 1)])
                    AddSegment(right.x, right.y, bottom.x, bottom.y);

                // South edge (bottom?left): neighbor y+1
                if (y == gridHeight - 1 || !blocksLoS[(y + 1) * gridWidth + x])
                    AddSegment(bottom.x, bottom.y, left.x, left.y);

                // West edge (left?top): neighbor x-1
                if (x == 0 || !blocksLoS[y * gridWidth + (x - 1)])
                    AddSegment(left.x, left.y, top.x, top.y);
            }
        }
    }

    // Get segments near a point (brute force for now; add spatial index later)
    std::vector<const OccluderSegment*> GetSegmentsInRadius(float cx, float cy,
                                                             float radius) const
    {
        std::vector<const OccluderSegment*> result;
        float r2 = radius * radius;
        for (const auto& seg : m_segments)
        {
            // Check if either endpoint is within radius
            float dx1 = seg.a.x - cx, dy1 = seg.a.y - cy;
            float dx2 = seg.b.x - cx, dy2 = seg.b.y - cy;
            if (dx1 * dx1 + dy1 * dy1 <= r2 || dx2 * dx2 + dy2 * dy2 <= r2)
            {
                result.push_back(&seg);
                continue;
            }

            // Check if closest point on segment is within radius
            float abx = seg.b.x - seg.a.x, aby = seg.b.y - seg.a.y;
            float apx = cx - seg.a.x, apy = cy - seg.a.y;
            float t = (apx * abx + apy * aby) / (abx * abx + aby * aby + 1e-10f);
            t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            float closestX = seg.a.x + t * abx;
            float closestY = seg.a.y + t * aby;
            float dcx = closestX - cx, dcy = closestY - cy;
            if (dcx * dcx + dcy * dcy <= r2)
                result.push_back(&seg);
        }
        return result;
    }

    const std::vector<OccluderSegment>& GetAllSegments() const { return m_segments; }
    size_t GetCount() const { return m_segments.size(); }

private:
    std::vector<OccluderSegment> m_segments;
};

} // namespace engine
