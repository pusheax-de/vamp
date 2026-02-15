#pragma once
// MapTile.h - Tile data for the 2D grid world

#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// Cover provided by a tile
// ---------------------------------------------------------------------------
enum class CoverLevel : uint8_t
{
    None,       // Open ground
    Half,       // Low wall, car, crate: -2 to attacker
    Full,       // Pillar, thick wall, dumpster: -4 to attacker
};

// ---------------------------------------------------------------------------
// Terrain type (affects movement, stealth, etc.)
// ---------------------------------------------------------------------------
enum class TerrainType : uint8_t
{
    Floor,          // Normal walkable
    Street,         // Open, vehicles
    Rubble,         // Costs extra AP to cross
    Water,          // Shallow (metro flooding), slow + noise
    Wall,           // Impassable, blocks LoS
    Door,           // Can be opened/closed/locked
    MetroTrack,     // Special metro rail tile
    Shadow,         // Dark area, stealth bonus
    COUNT
};

// ---------------------------------------------------------------------------
// A single tile on the map
// ---------------------------------------------------------------------------
struct MapTile
{
    TerrainType terrain     = TerrainType::Floor;
    CoverLevel  coverNorth  = CoverLevel::None;  // Cover provided to unit ON this tile vs attacks from north
    CoverLevel  coverSouth  = CoverLevel::None;
    CoverLevel  coverEast   = CoverLevel::None;
    CoverLevel  coverWest   = CoverLevel::None;
    bool        blocksLoS   = false;    // True for walls, closed doors
    bool        blocksMove  = false;    // True for walls
    bool        isShadow    = false;    // Stealth bonus
    bool        isLocked    = false;    // Doors: requires lockpicking / key
    int         moveCost    = 1;        // AP cost to enter (rubble = 2, water = 2)
    int8_t      elevation   = 0;        // Reserved for future (currently flat world)

    // Helpers
    static int  CoverHitPenalty(CoverLevel level);
    bool        IsPassable() const { return !blocksMove; }
    bool        BlocksSight() const { return blocksLoS; }
};

inline int MapTile::CoverHitPenalty(CoverLevel level)
{
    switch (level)
    {
    case CoverLevel::Half: return 2;
    case CoverLevel::Full: return 4;
    default:               return 0;
    }
}

} // namespace vamp
