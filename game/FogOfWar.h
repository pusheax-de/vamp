#pragma once
// FogOfWar.h - Vision, fog-of-war, and reveal mechanics

#include "GameWorld.h"
#include <vector>

namespace vamp
{

// ---------------------------------------------------------------------------
// Visibility state per tile (from player's perspective)
// ---------------------------------------------------------------------------
enum class Visibility : uint8_t
{
    Unknown,    // Never seen
    Explored,   // Previously seen, but not currently visible (greyed out)
    Visible,    // Currently in line-of-sight
};

// ---------------------------------------------------------------------------
// FogOfWar system
// ---------------------------------------------------------------------------
class FogOfWar
{
public:
    void Init(int mapWidth, int mapHeight);

    // Recalculate visibility from a character's position and vision radius
    void UpdateVision(const GameMap& map, int observerX, int observerY, int visionRadius);

    // Reveal a radius around a point (e.g., Auspex pulse)
    void RevealRadius(const GameMap& map, int cx, int cy, int radius);

    // Mark a specific tile as visible (e.g., Blood Mark tracking)
    void RevealTile(int x, int y);

    // Reset all Visible tiles to Explored (call before recalculating)
    void ClearVisible();

    // Query
    Visibility GetVisibility(int x, int y) const;
    bool       IsVisible(int x, int y) const;
    bool       IsExplored(int x, int y) const;

    // Get all currently visible enemy positions (for AI / UI)
    std::vector<Character*> GetVisibleEnemies(GameWorld& world, Faction playerFaction);

private:
    int                     m_width  = 0;
    int                     m_height = 0;
    std::vector<Visibility> m_grid;
};

} // namespace vamp
