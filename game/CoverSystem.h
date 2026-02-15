#pragma once
// CoverSystem.h - Cover mechanics: query cover between positions, flanking

#include "MapTile.h"
#include <vector>

namespace vamp
{

// Forward declaration
struct GameMap;

// ---------------------------------------------------------------------------
// Direction of attack relative to target
// ---------------------------------------------------------------------------
enum class AttackDirection : uint8_t
{
    North,
    South,
    East,
    West,
    Diagonal,   // No directional cover bonus (flanking!)
};

// ---------------------------------------------------------------------------
// Cover query result
// ---------------------------------------------------------------------------
struct CoverInfo
{
    CoverLevel  level       = CoverLevel::None;
    int         hitPenalty  = 0;     // Penalty applied to attacker
    bool        isFlanked   = false; // Target has no cover from this direction
};

// ---------------------------------------------------------------------------
// CoverSystem — static utility functions for cover mechanics
// ---------------------------------------------------------------------------
namespace CoverSystem
{
    // Determine attack direction from attacker to target
    AttackDirection GetAttackDirection(int attackerX, int attackerY,
                                       int targetX, int targetY);

    // Query cover the target has against an attack from the given direction
    CoverInfo QueryCover(const MapTile& targetTile, AttackDirection dir);

    // Full query: given attacker & target positions and the map
    CoverInfo QueryCoverBetween(const GameMap& map,
                                 int attackerX, int attackerY,
                                 int targetX, int targetY);

    // Check line-of-sight between two points (Bresenham)
    bool HasLineOfSight(const GameMap& map,
                        int x0, int y0, int x1, int y1);

    // Calculate distance in tiles (Chebyshev / king-move distance for 2D grid)
    int TileDistance(int x0, int y0, int x1, int y1);

} // namespace CoverSystem

} // namespace vamp
