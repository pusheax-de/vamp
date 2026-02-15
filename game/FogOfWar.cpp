// FogOfWar.cpp - Vision and fog-of-war implementation

#include "FogOfWar.h"
#include "CoverSystem.h"
#include <cmath>

namespace vamp
{

void FogOfWar::Init(int mapWidth, int mapHeight)
{
    m_width  = mapWidth;
    m_height = mapHeight;
    m_grid.assign(mapWidth * mapHeight, Visibility::Unknown);
}

void FogOfWar::UpdateVision(const GameMap& map, int observerX, int observerY, int visionRadius)
{
    ClearVisible();

    for (int dy = -visionRadius; dy <= visionRadius; ++dy)
    {
        for (int dx = -visionRadius; dx <= visionRadius; ++dx)
        {
            int tx = observerX + dx;
            int ty = observerY + dy;
            if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height)
                continue;
            if (CoverSystem::TileDistance(observerX, observerY, tx, ty) > visionRadius)
                continue;
            if (CoverSystem::HasLineOfSight(map, observerX, observerY, tx, ty))
            {
                m_grid[ty * m_width + tx] = Visibility::Visible;
            }
        }
    }
}

void FogOfWar::RevealRadius(const GameMap& map, int cx, int cy, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int tx = cx + dx;
            int ty = cy + dy;
            if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height)
                continue;
            if (CoverSystem::TileDistance(cx, cy, tx, ty) <= radius)
            {
                if (CoverSystem::HasLineOfSight(map, cx, cy, tx, ty))
                    m_grid[ty * m_width + tx] = Visibility::Visible;
            }
        }
    }
}

void FogOfWar::RevealTile(int x, int y)
{
    if (x >= 0 && x < m_width && y >= 0 && y < m_height)
        m_grid[y * m_width + x] = Visibility::Visible;
}

void FogOfWar::ClearVisible()
{
    for (auto& v : m_grid)
    {
        if (v == Visibility::Visible)
            v = Visibility::Explored;
    }
}

Visibility FogOfWar::GetVisibility(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return Visibility::Unknown;
    return m_grid[y * m_width + x];
}

bool FogOfWar::IsVisible(int x, int y) const
{
    return GetVisibility(x, y) == Visibility::Visible;
}

bool FogOfWar::IsExplored(int x, int y) const
{
    Visibility v = GetVisibility(x, y);
    return v == Visibility::Explored || v == Visibility::Visible;
}

std::vector<Character*> FogOfWar::GetVisibleEnemies(GameWorld& world, Faction playerFaction)
{
    std::vector<Character*> result;
    for (auto& c : world.characters)
    {
        if (!c.isAlive) continue;
        if (c.faction == playerFaction) continue;
        if (IsVisible(c.tileX, c.tileY))
        {
            // If target is obfuscated, they might still be invisible
            if (!c.statuses.Has(StatusType::Obfuscated))
                result.push_back(&c);
        }
    }
    return result;
}

} // namespace vamp
