// SceneFile.cpp - Binary serialization/deserialization for .vmp scene files
//
// Format layout (all little-endian):
//   [SceneHeader]
//   [uint32 tileCount] [MapTile * tileCount]
//   [uint32 bgPageCount] [SceneBackgroundPage * count]
//   [int32 playerSpawnX] [int32 playerSpawnY]
//   [uint32 npcCount] [for each: NPC blob]
//   [uint32 patrolRouteCount] [for each: route blob]
//   [uint32 groundItemCount] [SceneGroundItem * count]
//   [uint32 questItemCount] [SceneQuestItem * count]
//   [uint32 triggerCount] [SceneTrigger * count]
//   [uint32 transitionCount] [SceneTransition * count]
//   [uint32 shopCount] [SceneShop * count]
//   [uint32 safehouseCount] [for each: safehouse blob]
//   [uint32 lightCount] [SceneLight * count]
//   [uint32 roofCount] [SceneRoof * count]
//   [uint32 territoryCount] [for each: territory blob]
//   [int32 globalHeat] [int32 dayCount]

#include "SceneFile.h"
#include <fstream>
#include <cstring>

namespace vamp
{

// ===========================================================================
// Helpers: write/read primitives
// ===========================================================================

namespace
{

template<typename T>
void WriteRaw(std::ofstream& f, const T& val)
{
    f.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

template<typename T>
bool ReadRaw(std::ifstream& f, T& val)
{
    f.read(reinterpret_cast<char*>(&val), sizeof(T));
    return f.good();
}

void WriteString(std::ofstream& f, const std::string& s)
{
    uint32_t len = static_cast<uint32_t>(s.size());
    WriteRaw(f, len);
    if (len > 0)
        f.write(s.data(), len);
}

bool ReadString(std::ifstream& f, std::string& s)
{
    uint32_t len = 0;
    if (!ReadRaw(f, len)) return false;
    if (len > 0)
    {
        s.resize(len);
        f.read(&s[0], len);
    }
    else
    {
        s.clear();
    }
    return f.good();
}

void WriteCharacter(std::ofstream& f, const Character& c)
{
    WriteString(f, c.name);
    WriteRaw(f, c.faction);
    WriteRaw(f, c.isVampire);
    WriteRaw(f, c.isAlive);

    // Attributes (6 ints)
    for (int i = 0; i < ATTR_COUNT; ++i)
        WriteRaw(f, c.attributes.values[i]);

    // Skills (26 ints)
    for (int i = 0; i < SKILL_COUNT; ++i)
        WriteRaw(f, c.skills.ranks[i]);

    WriteRaw(f, c.currentHP);
    WriteRaw(f, c.currentAP);
    WriteRaw(f, c.bloodReserve);
    WriteRaw(f, c.maxBR);
    WriteRaw(f, c.bloodPotency);

    // Status effects
    for (int i = 0; i < STATUS_COUNT; ++i)
    {
        WriteRaw(f, c.statuses.effects[i].type);
        WriteRaw(f, c.statuses.effects[i].duration);
        WriteRaw(f, c.statuses.effects[i].potency);
        WriteRaw(f, c.statuses.effects[i].active);
    }

    // Equipment
    WriteRaw(f, c.inventory.equipment.primaryWeapon);
    WriteRaw(f, c.inventory.equipment.secondaryWeapon);
    WriteRaw(f, c.inventory.equipment.armor);
    WriteRaw(f, c.inventory.equipment.primaryAmmo);
    WriteRaw(f, c.inventory.equipment.secondaryAmmo);

    // Backpack
    uint32_t itemCount = static_cast<uint32_t>(c.inventory.backpack.size());
    WriteRaw(f, itemCount);
    for (const auto& item : c.inventory.backpack)
    {
        WriteRaw(f, item.type);
        WriteRaw(f, item.templateId);
        WriteRaw(f, item.quantity);
    }
    WriteRaw(f, c.inventory.money);

    WriteRaw(f, c.tileX);
    WriteRaw(f, c.tileY);
    WriteRaw(f, c.isHidden);
    WriteRaw(f, c.noiseLevel);
}

bool ReadCharacter(std::ifstream& f, Character& c)
{
    if (!ReadString(f, c.name)) return false;
    if (!ReadRaw(f, c.faction)) return false;
    if (!ReadRaw(f, c.isVampire)) return false;
    if (!ReadRaw(f, c.isAlive)) return false;

    for (int i = 0; i < ATTR_COUNT; ++i)
        if (!ReadRaw(f, c.attributes.values[i])) return false;

    for (int i = 0; i < SKILL_COUNT; ++i)
        if (!ReadRaw(f, c.skills.ranks[i])) return false;

    if (!ReadRaw(f, c.currentHP)) return false;
    if (!ReadRaw(f, c.currentAP)) return false;
    if (!ReadRaw(f, c.bloodReserve)) return false;
    if (!ReadRaw(f, c.maxBR)) return false;
    if (!ReadRaw(f, c.bloodPotency)) return false;

    for (int i = 0; i < STATUS_COUNT; ++i)
    {
        if (!ReadRaw(f, c.statuses.effects[i].type)) return false;
        if (!ReadRaw(f, c.statuses.effects[i].duration)) return false;
        if (!ReadRaw(f, c.statuses.effects[i].potency)) return false;
        if (!ReadRaw(f, c.statuses.effects[i].active)) return false;
    }

    if (!ReadRaw(f, c.inventory.equipment.primaryWeapon)) return false;
    if (!ReadRaw(f, c.inventory.equipment.secondaryWeapon)) return false;
    if (!ReadRaw(f, c.inventory.equipment.armor)) return false;
    if (!ReadRaw(f, c.inventory.equipment.primaryAmmo)) return false;
    if (!ReadRaw(f, c.inventory.equipment.secondaryAmmo)) return false;

    uint32_t itemCount = 0;
    if (!ReadRaw(f, itemCount)) return false;
    c.inventory.backpack.resize(itemCount);
    for (uint32_t i = 0; i < itemCount; ++i)
    {
        if (!ReadRaw(f, c.inventory.backpack[i].type)) return false;
        if (!ReadRaw(f, c.inventory.backpack[i].templateId)) return false;
        if (!ReadRaw(f, c.inventory.backpack[i].quantity)) return false;
        c.inventory.backpack[i].name = ""; // Resolved from templateId at runtime
    }
    if (!ReadRaw(f, c.inventory.money)) return false;

    if (!ReadRaw(f, c.tileX)) return false;
    if (!ReadRaw(f, c.tileY)) return false;
    if (!ReadRaw(f, c.isHidden)) return false;
    if (!ReadRaw(f, c.noiseLevel)) return false;

    return true;
}

void WriteSafehouse(std::ofstream& f, const Safehouse& sh)
{
    WriteString(f, sh.name);
    WriteRaw(f, sh.tileX);
    WriteRaw(f, sh.tileY);
    WriteRaw(f, sh.securityRating);
    WriteRaw(f, sh.accessCost);
    WriteRaw(f, sh.controlledBy);
    WriteRaw(f, sh.isDiscovered);
    WriteRaw(f, sh.isAvailable);
}

bool ReadSafehouse(std::ifstream& f, Safehouse& sh)
{
    if (!ReadString(f, sh.name)) return false;
    if (!ReadRaw(f, sh.tileX)) return false;
    if (!ReadRaw(f, sh.tileY)) return false;
    if (!ReadRaw(f, sh.securityRating)) return false;
    if (!ReadRaw(f, sh.accessCost)) return false;
    if (!ReadRaw(f, sh.controlledBy)) return false;
    if (!ReadRaw(f, sh.isDiscovered)) return false;
    if (!ReadRaw(f, sh.isAvailable)) return false;
    return true;
}

void WriteTerritory(std::ofstream& f, const Territory& t)
{
    WriteString(f, t.name);
    WriteRaw(f, t.controllingFaction);
    WriteRaw(f, t.controlStrength);
    WriteRaw(f, t.heatLevel);
    WriteRaw(f, t.dangerRating);
    WriteRaw(f, t.x0);
    WriteRaw(f, t.y0);
    WriteRaw(f, t.x1);
    WriteRaw(f, t.y1);
}

bool ReadTerritory(std::ifstream& f, Territory& t)
{
    if (!ReadString(f, t.name)) return false;
    if (!ReadRaw(f, t.controllingFaction)) return false;
    if (!ReadRaw(f, t.controlStrength)) return false;
    if (!ReadRaw(f, t.heatLevel)) return false;
    if (!ReadRaw(f, t.dangerRating)) return false;
    if (!ReadRaw(f, t.x0)) return false;
    if (!ReadRaw(f, t.y0)) return false;
    if (!ReadRaw(f, t.x1)) return false;
    if (!ReadRaw(f, t.y1)) return false;
    return true;
}

} // anonymous namespace

// ===========================================================================
// Save
// ===========================================================================

bool SceneFile::Save(const std::string& filePath, const SceneData& scene)
{
    std::ofstream f(filePath, std::ios::binary);
    if (!f.is_open())
        return false;

    // Header
    WriteRaw(f, scene.header);

    // Tiles
    uint32_t tileCount = static_cast<uint32_t>(scene.tiles.size());
    WriteRaw(f, tileCount);
    for (const auto& tile : scene.tiles)
        WriteRaw(f, tile);

    // Background pages
    uint32_t bgCount = static_cast<uint32_t>(scene.backgroundPages.size());
    WriteRaw(f, bgCount);
    for (const auto& page : scene.backgroundPages)
        WriteRaw(f, page);

    // Player spawn
    WriteRaw(f, scene.playerSpawnX);
    WriteRaw(f, scene.playerSpawnY);

    // NPCs
    uint32_t npcCount = static_cast<uint32_t>(scene.npcs.size());
    WriteRaw(f, npcCount);
    for (const auto& npc : scene.npcs)
    {
        WriteCharacter(f, npc.character);
        WriteRaw(f, npc.behavior);
        WriteRaw(f, npc.dialogueId);
        WriteRaw(f, npc.patrolRouteId);
        WriteRaw(f, npc.isHostile);
        WriteRaw(f, npc.isEssential);
        f.write(npc.tag, sizeof(npc.tag));
    }

    // Patrol routes
    uint32_t routeCount = static_cast<uint32_t>(scene.patrolRoutes.size());
    WriteRaw(f, routeCount);
    for (const auto& route : scene.patrolRoutes)
    {
        WriteRaw(f, route.routeId);
        WriteRaw(f, route.loops);
        uint32_t wpCount = static_cast<uint32_t>(route.waypoints.size());
        WriteRaw(f, wpCount);
        for (const auto& wp : route.waypoints)
            WriteRaw(f, wp);
    }

    // Ground items
    uint32_t giCount = static_cast<uint32_t>(scene.groundItems.size());
    WriteRaw(f, giCount);
    for (const auto& gi : scene.groundItems)
        WriteRaw(f, gi);

    // Quest items
    uint32_t qiCount = static_cast<uint32_t>(scene.questItems.size());
    WriteRaw(f, qiCount);
    for (const auto& qi : scene.questItems)
        WriteRaw(f, qi);

    // Triggers
    uint32_t trgCount = static_cast<uint32_t>(scene.triggers.size());
    WriteRaw(f, trgCount);
    for (const auto& trg : scene.triggers)
        WriteRaw(f, trg);

    // Transitions
    uint32_t transCount = static_cast<uint32_t>(scene.transitions.size());
    WriteRaw(f, transCount);
    for (const auto& trans : scene.transitions)
        WriteRaw(f, trans);

    // Shops
    uint32_t shopCount = static_cast<uint32_t>(scene.shops.size());
    WriteRaw(f, shopCount);
    for (const auto& shop : scene.shops)
        WriteRaw(f, shop);

    // Safehouses
    uint32_t shCount = static_cast<uint32_t>(scene.safehouses.size());
    WriteRaw(f, shCount);
    for (const auto& sh : scene.safehouses)
        WriteSafehouse(f, sh);

    // Lights
    uint32_t lightCount = static_cast<uint32_t>(scene.lights.size());
    WriteRaw(f, lightCount);
    for (const auto& light : scene.lights)
        WriteRaw(f, light);

    // Roofs
    uint32_t roofCount = static_cast<uint32_t>(scene.roofs.size());
    WriteRaw(f, roofCount);
    for (const auto& roof : scene.roofs)
        WriteRaw(f, roof);

    // Territories
    uint32_t terCount = static_cast<uint32_t>(scene.territories.size());
    WriteRaw(f, terCount);
    for (const auto& ter : scene.territories)
        WriteTerritory(f, ter);

    // Global state
    WriteRaw(f, scene.globalHeat);
    WriteRaw(f, scene.dayCount);

    return f.good();
}

// ===========================================================================
// Load
// ===========================================================================

bool SceneFile::Load(const std::string& filePath, SceneData& scene)
{
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open())
        return false;

    // Header
    if (!ReadRaw(f, scene.header))
        return false;

    if (scene.header.magic != kSceneMagic)
        return false;
    if (scene.header.version != kSceneVersion)
        return false;

    // Tiles
    uint32_t tileCount = 0;
    if (!ReadRaw(f, tileCount)) return false;
    scene.tiles.resize(tileCount);
    for (uint32_t i = 0; i < tileCount; ++i)
        if (!ReadRaw(f, scene.tiles[i])) return false;

    // Background pages
    uint32_t bgCount = 0;
    if (!ReadRaw(f, bgCount)) return false;
    scene.backgroundPages.resize(bgCount);
    for (uint32_t i = 0; i < bgCount; ++i)
        if (!ReadRaw(f, scene.backgroundPages[i])) return false;

    // Player spawn
    if (!ReadRaw(f, scene.playerSpawnX)) return false;
    if (!ReadRaw(f, scene.playerSpawnY)) return false;

    // NPCs
    uint32_t npcCount = 0;
    if (!ReadRaw(f, npcCount)) return false;
    scene.npcs.resize(npcCount);
    for (uint32_t i = 0; i < npcCount; ++i)
    {
        if (!ReadCharacter(f, scene.npcs[i].character)) return false;
        if (!ReadRaw(f, scene.npcs[i].behavior)) return false;
        if (!ReadRaw(f, scene.npcs[i].dialogueId)) return false;
        if (!ReadRaw(f, scene.npcs[i].patrolRouteId)) return false;
        if (!ReadRaw(f, scene.npcs[i].isHostile)) return false;
        if (!ReadRaw(f, scene.npcs[i].isEssential)) return false;
        f.read(scene.npcs[i].tag, sizeof(scene.npcs[i].tag));
        if (!f.good()) return false;
    }

    // Patrol routes
    uint32_t routeCount = 0;
    if (!ReadRaw(f, routeCount)) return false;
    scene.patrolRoutes.resize(routeCount);
    for (uint32_t i = 0; i < routeCount; ++i)
    {
        if (!ReadRaw(f, scene.patrolRoutes[i].routeId)) return false;
        if (!ReadRaw(f, scene.patrolRoutes[i].loops)) return false;
        uint32_t wpCount = 0;
        if (!ReadRaw(f, wpCount)) return false;
        scene.patrolRoutes[i].waypoints.resize(wpCount);
        for (uint32_t j = 0; j < wpCount; ++j)
            if (!ReadRaw(f, scene.patrolRoutes[i].waypoints[j])) return false;
    }

    // Ground items
    uint32_t giCount = 0;
    if (!ReadRaw(f, giCount)) return false;
    scene.groundItems.resize(giCount);
    for (uint32_t i = 0; i < giCount; ++i)
        if (!ReadRaw(f, scene.groundItems[i])) return false;

    // Quest items
    uint32_t qiCount = 0;
    if (!ReadRaw(f, qiCount)) return false;
    scene.questItems.resize(qiCount);
    for (uint32_t i = 0; i < qiCount; ++i)
        if (!ReadRaw(f, scene.questItems[i])) return false;

    // Triggers
    uint32_t trgCount = 0;
    if (!ReadRaw(f, trgCount)) return false;
    scene.triggers.resize(trgCount);
    for (uint32_t i = 0; i < trgCount; ++i)
        if (!ReadRaw(f, scene.triggers[i])) return false;

    // Transitions
    uint32_t transCount = 0;
    if (!ReadRaw(f, transCount)) return false;
    scene.transitions.resize(transCount);
    for (uint32_t i = 0; i < transCount; ++i)
        if (!ReadRaw(f, scene.transitions[i])) return false;

    // Shops
    uint32_t shopCount = 0;
    if (!ReadRaw(f, shopCount)) return false;
    scene.shops.resize(shopCount);
    for (uint32_t i = 0; i < shopCount; ++i)
        if (!ReadRaw(f, scene.shops[i])) return false;

    // Safehouses
    uint32_t shCount = 0;
    if (!ReadRaw(f, shCount)) return false;
    scene.safehouses.resize(shCount);
    for (uint32_t i = 0; i < shCount; ++i)
        if (!ReadSafehouse(f, scene.safehouses[i])) return false;

    // Lights
    uint32_t lightCount = 0;
    if (!ReadRaw(f, lightCount)) return false;
    scene.lights.resize(lightCount);
    for (uint32_t i = 0; i < lightCount; ++i)
        if (!ReadRaw(f, scene.lights[i])) return false;

    // Roofs
    uint32_t roofCount = 0;
    if (!ReadRaw(f, roofCount)) return false;
    scene.roofs.resize(roofCount);
    for (uint32_t i = 0; i < roofCount; ++i)
        if (!ReadRaw(f, scene.roofs[i])) return false;

    // Territories
    uint32_t terCount = 0;
    if (!ReadRaw(f, terCount)) return false;
    scene.territories.resize(terCount);
    for (uint32_t i = 0; i < terCount; ++i)
        if (!ReadTerritory(f, scene.territories[i])) return false;

    // Global state
    if (!ReadRaw(f, scene.globalHeat)) return false;
    if (!ReadRaw(f, scene.dayCount)) return false;

    return true;
}

} // namespace vamp
