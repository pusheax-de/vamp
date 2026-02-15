// CoverSystem.cpp - Cover mechanics implementation

#include "CoverSystem.h"
#include "GameWorld.h"
#include <cmath>
#include <algorithm>

namespace vamp
{

namespace CoverSystem
{

AttackDirection GetAttackDirection(int attackerX, int attackerY,
                                   int targetX, int targetY)
{
    int dx = targetX - attackerX;
    int dy = targetY - attackerY;

    // If diagonal, no directional cover
    if (dx != 0 && dy != 0)
        return AttackDirection::Diagonal;

    if (dy < 0) return AttackDirection::South;  // Attacker is south, attacks toward north
    if (dy > 0) return AttackDirection::North;  // Attacker is north, attacks toward south
    if (dx < 0) return AttackDirection::East;   // Attacker is east
    if (dx > 0) return AttackDirection::West;   // Attacker is west

    return AttackDirection::Diagonal; // Same tile (shouldn't happen)
}

CoverInfo QueryCover(const MapTile& targetTile, AttackDirection dir)
{
    CoverInfo info;

    CoverLevel level = CoverLevel::None;
    switch (dir)
    {
    case AttackDirection::North: level = targetTile.coverNorth; break;
    case AttackDirection::South: level = targetTile.coverSouth; break;
    case AttackDirection::East:  level = targetTile.coverEast;  break;
    case AttackDirection::West:  level = targetTile.coverWest;  break;
    case AttackDirection::Diagonal:
        // Diagonal attacks: use the best cover from adjacent sides
        level = std::max(targetTile.coverNorth,
                std::max(targetTile.coverSouth,
                std::max(targetTile.coverEast, targetTile.coverWest)));
        // But reduce by one step for diagonal (less effective)
        if (level == CoverLevel::Full)
            level = CoverLevel::Half;
        else
            level = CoverLevel::None;
        break;
    }

    info.level      = level;
    info.hitPenalty  = MapTile::CoverHitPenalty(level);
    info.isFlanked   = (level == CoverLevel::None);

    return info;
}

CoverInfo QueryCoverBetween(const GameMap& map,
                             int attackerX, int attackerY,
                             int targetX, int targetY)
{
    if (!map.InBounds(targetX, targetY))
        return CoverInfo{};

    AttackDirection dir = GetAttackDirection(attackerX, attackerY, targetX, targetY);
    return QueryCover(map.At(targetX, targetY), dir);
}

bool HasLineOfSight(const GameMap& map, int x0, int y0, int x1, int y1)
{
    // Bresenham line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int cx = x0;
    int cy = y0;

    while (cx != x1 || cy != y1)
    {
        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            cx += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            cy += sy;
        }

        // Don't check the start or end tile for LoS blocking
        if (cx == x1 && cy == y1) break;

        if (map.InBounds(cx, cy) && map.At(cx, cy).BlocksSight())
            return false;
    }
    return true;
}

int TileDistance(int x0, int y0, int x1, int y1)
{
    return std::max(std::abs(x1 - x0), std::abs(y1 - y0));
}

} // namespace CoverSystem

} // namespace vamp
