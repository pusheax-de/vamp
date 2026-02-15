#pragma once
// GameWorld.h - Grid map, territory control, clans, heat system

#include "MapTile.h"
#include "Character.h"
#include <vector>
#include <string>

namespace vamp
{

// ---------------------------------------------------------------------------
// GameMap - 2D tile grid
// ---------------------------------------------------------------------------
struct GameMap
{
    int                     width  = 0;
    int                     height = 0;
    std::vector<MapTile>    tiles;

    void        Init(int w, int h);
    MapTile&    At(int x, int y);
    const MapTile& At(int x, int y) const;
    bool        InBounds(int x, int y) const;
};

// ---------------------------------------------------------------------------
// Territory - a named area controlled by a faction
// ---------------------------------------------------------------------------
struct Territory
{
    std::string name;
    Faction     controllingFaction  = Faction::Civilian;
    int         controlStrength     = 0;    // 0-100
    int         heatLevel           = 0;    // Police/merc attention 0-100
    int         dangerRating        = 0;    // General danger for sleeping, etc.

    // Bounding box on the map
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    bool ContainsTile(int x, int y) const;
};

// ---------------------------------------------------------------------------
// Clan data
// ---------------------------------------------------------------------------
struct ClanInfo
{
    Faction     faction;
    std::string name;
    std::string description;

    // Clan bonuses: which disciplines are favored
    SkillId     favoredDiscipline1 = SkillId::Blink;
    SkillId     favoredDiscipline2 = SkillId::Auspex;

    // Reputation with this clan: -100 (hated) to +100 (allied)
    int         playerReputation   = 0;
};

// ---------------------------------------------------------------------------
// GameWorld - holds map, territories, clans, global state
// ---------------------------------------------------------------------------
struct GameWorld
{
    GameMap                     map;
    std::vector<Territory>      territories;
    std::vector<ClanInfo>       clans;
    std::vector<Character>      characters;     // All characters in the world

    int     globalHeat      = 0;    // City-wide police alert level
    int     dayCount        = 0;    // In-game day counter

    // Initialization
    void    InitClans();

    // Territory queries
    const Territory*    GetTerritoryAt(int x, int y) const;
    void                AddHeat(int x, int y, int amount);

    // Character management
    Character*          GetCharacterAt(int x, int y);
    Character*          FindCharacterByName(const std::string& name);
    void                RemoveDeadCharacters();
    std::vector<Character*> GetCharactersInRadius(int cx, int cy, int radius);
    std::vector<Character*> GetCharactersByFaction(Faction faction);

    // Day cycle
    void    AdvanceDay();
};

} // namespace vamp
