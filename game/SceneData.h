#pragma once
// SceneData.h - Complete scene/level data for the .vmp binary format
//
// A scene contains everything needed to fully describe one playable map:
//   - Grid dimensions and tile data (terrain, cover, LoS, walkability)
//   - Background page layout
//   - NPC placements with full character state
//   - Player spawn location
//   - Item pickups on the ground
//   - Trigger zones (scripts, traps, dialogues)
//   - Transition zones (doors/stairs that load another scene)
//   - Shop locations
//   - Quest item locations
//   - Safehouse locations
//   - Light placements
//   - Territory metadata
//
// Binary format identifier: "VMP1" (4 bytes)

#include "MapTile.h"
#include "Character.h"
#include "Inventory.h"
#include "SleepSystem.h"
#include "GameWorld.h"
#include <string>
#include <vector>
#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// File header
// ---------------------------------------------------------------------------
static const uint32_t kSceneMagic   = 0x31504D56; // "VMP1" in little-endian
static const uint32_t kSceneVersion = 2;

struct SceneHeader
{
    uint32_t    magic       = kSceneMagic;
    uint32_t    version     = kSceneVersion;
    uint32_t    gridWidth   = 0;
    uint32_t    gridHeight  = 0;
    float       tileSize    = 32.0f;
    float       originX     = 0.0f;
    float       originY     = 0.0f;
    char        sceneName[64] = {};
};

// ---------------------------------------------------------------------------
// Background page reference (which DDS files tile the background)
// ---------------------------------------------------------------------------
struct SceneBackgroundPage
{
    int32_t     pageX       = 0;
    int32_t     pageY       = 0;
    char        filePath[128] = {};     // Relative path to DDS file
};

// ---------------------------------------------------------------------------
// An item lying on the ground (pickup)
// ---------------------------------------------------------------------------
struct SceneGroundItem
{
    int32_t     tileX       = 0;
    int32_t     tileY       = 0;
    ItemType    type        = ItemType::Misc;
    uint16_t    templateId  = 0;
    int32_t     quantity    = 1;
};

// ---------------------------------------------------------------------------
// Trigger zone - an area that fires a game event
// ---------------------------------------------------------------------------
enum class TriggerType : uint8_t
{
    Script,         // Generic script callback
    Trap,           // Triggers a trap (damage, alarm, etc.)
    Dialogue,       // Starts a dialogue sequence
    Ambush,         // Spawns enemies
    LorePickup,     // Reveals lore / journal entry
    Alarm,          // Triggers an alarm (adds heat)
    COUNT
};

struct SceneTrigger
{
    int32_t     x0 = 0, y0 = 0, x1 = 0, y1 = 0; // Bounding box in tiles
    TriggerType type        = TriggerType::Script;
    bool        oneShot     = true;     // Only fire once per playthrough
    bool        enabled     = true;
    uint32_t    scriptId    = 0;        // Index into script table
    char        tag[32]     = {};       // Identifier for game logic
};

// ---------------------------------------------------------------------------
// Transition zone - loads another scene
// ---------------------------------------------------------------------------
struct SceneTransition
{
    int32_t     x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    char        targetScene[64] = {};   // Filename of the target .vmp
    int32_t     targetSpawnX = 0;       // Where to place the player in the new scene
    int32_t     targetSpawnY = 0;
    bool        requiresKey = false;
    uint16_t    keyItemId   = 0;        // Item templateId needed to pass
    char        tag[32]     = {};
};

// ---------------------------------------------------------------------------
// Shop location
// ---------------------------------------------------------------------------
struct SceneShop
{
    int32_t     tileX       = 0;
    int32_t     tileY       = 0;
    char        shopName[64] = {};
    Faction     faction     = Faction::Civilian;
    uint32_t    inventoryId = 0;        // References external shop inventory data
    char        tag[32]     = {};
};

// ---------------------------------------------------------------------------
// Quest item location (a special ground item with quest metadata)
// ---------------------------------------------------------------------------
struct SceneQuestItem
{
    int32_t     tileX       = 0;
    int32_t     tileY       = 0;
    uint16_t    questId     = 0;
    uint16_t    templateId  = 0;
    bool        collected   = false;
    char        tag[32]     = {};
};

// ---------------------------------------------------------------------------
// Light placement in the scene
// ---------------------------------------------------------------------------
struct SceneLight
{
    float       worldX      = 0.0f;
    float       worldY      = 0.0f;
    float       r = 1.0f, g = 1.0f, b = 1.0f;
    float       radius      = 100.0f;
    float       intensity   = 1.0f;
    float       flickerPhase = 0.0f;
    char        tag[32]     = {};
};

// ---------------------------------------------------------------------------
// NPC placement - a full character with scene-specific state
// ---------------------------------------------------------------------------
enum class NPCBehavior : uint8_t
{
    Idle,           // Stands in place
    Patrol,         // Walks a route
    Guard,          // Attacks hostiles on sight
    Merchant,       // Can be traded with
    QuestGiver,     // Has a dialogue / quest
    Civilian,       // Flees from combat
    COUNT
};

struct SceneNPC
{
    Character   character;              // Full character sheet
    NPCBehavior behavior    = NPCBehavior::Idle;
    uint32_t    dialogueId  = 0;        // Dialogue tree reference
    uint32_t    patrolRouteId = 0;      // Patrol route reference (if Patrol)
    bool        isHostile   = false;
    bool        isEssential = false;    // Cannot be killed (story NPCs)
    char        tag[32]     = {};       // Unique identifier for scripts
};

// ---------------------------------------------------------------------------
// Patrol route (sequence of waypoints for patrolling NPCs)
// ---------------------------------------------------------------------------
struct PatrolWaypoint
{
    int32_t tileX = 0;
    int32_t tileY = 0;
    float   waitTime = 0.0f;           // Seconds to pause at this waypoint
};

struct PatrolRoute
{
    uint32_t                    routeId = 0;
    bool                        loops   = true;
    std::vector<PatrolWaypoint> waypoints;
};

// ---------------------------------------------------------------------------
// Roof zone (building interiors)
// ---------------------------------------------------------------------------
struct SceneRoof
{
    int32_t     x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    uint16_t    textureId   = 0;        // Atlas/texture reference
    char        tag[32]     = {};
};

// ---------------------------------------------------------------------------
// Placed object covering a bounding box of tiles (e.g. hangar, building)
// ---------------------------------------------------------------------------
enum class SceneObjectType : uint8_t
{
    Hangar,
    COUNT
};

struct SceneObject
{
    SceneObjectType type    = SceneObjectType::Hangar;
    int32_t         x0 = 0, y0 = 0, x1 = 0, y1 = 0; // Tile bounding box
    char            imagePath[128] = {};               // Relative path to PNG
    char            tag[32]        = {};
};

// ---------------------------------------------------------------------------
// SceneData - the complete in-memory representation of a .vmp scene
// ---------------------------------------------------------------------------
struct SceneData
{
    SceneHeader                     header;

    // Tile grid (row-major, size = header.gridWidth * header.gridHeight)
    std::vector<MapTile>            tiles;

    // Background pages
    std::vector<SceneBackgroundPage> backgroundPages;

    // Background image (single image covering the level, relative path to PNG)
    std::string                     backgroundImagePath;

    // Player spawn
    int32_t                         playerSpawnX = 0;
    int32_t                         playerSpawnY = 0;

    // Entities
    std::vector<SceneNPC>           npcs;
    std::vector<PatrolRoute>        patrolRoutes;

    // World items
    std::vector<SceneGroundItem>    groundItems;
    std::vector<SceneQuestItem>     questItems;

    // Placed objects (hangars, buildings, etc.)
    std::vector<SceneObject>        objects;

    // Zones
    std::vector<SceneTrigger>       triggers;
    std::vector<SceneTransition>    transitions;

    // Locations
    std::vector<SceneShop>          shops;
    std::vector<Safehouse>          safehouses;

    // Rendering
    std::vector<SceneLight>         lights;
    std::vector<SceneRoof>          roofs;

    // Territories
    std::vector<Territory>          territories;

    // Scene-level state
    int32_t                         globalHeat = 0;
    int32_t                         dayCount   = 0;

    // Helpers
    const MapTile& TileAt(int x, int y) const;
    MapTile&       TileAt(int x, int y);
    bool           InBounds(int x, int y) const;
};

// ---------------------------------------------------------------------------
// Inline helpers
// ---------------------------------------------------------------------------

inline const MapTile& SceneData::TileAt(int x, int y) const
{
    return tiles[y * static_cast<int>(header.gridWidth) + x];
}

inline MapTile& SceneData::TileAt(int x, int y)
{
    return tiles[y * static_cast<int>(header.gridWidth) + x];
}

inline bool SceneData::InBounds(int x, int y) const
{
    return x >= 0 && x < static_cast<int>(header.gridWidth) &&
           y >= 0 && y < static_cast<int>(header.gridHeight);
}

} // namespace vamp
