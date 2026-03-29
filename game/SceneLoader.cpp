// SceneLoader.cpp - Loads .vmp scene into engine and game systems

#include "SceneLoader.h"

// Engine headers
#include "../engine/Grid.h"
#include "../engine/OccluderSet.h"
#include "../engine/LightSystem.h"
#include "../engine/FogRenderer.h"
#include "../engine/RoofSystem.h"
#include "../engine/Camera2D.h"
#include "../engine/EngineTypes.h"

namespace vamp
{

// ===========================================================================
// LoadScene
// ===========================================================================

bool SceneLoader::LoadScene(const std::string& filePath,
                             engine::Grid& grid,
                             engine::OccluderSet& occluders,
                             engine::LightSystem& lights,
                             engine::FogRenderer& fog,
                             engine::RoofSystem& roofs,
                             engine::Camera2D& camera)
{
    UnloadScene();

    if (!SceneFile::Load(filePath, m_scene))
        return false;

    const auto& hdr = m_scene.header;

    // --- Set up engine grid ---
    grid.Init(hdr.tileSize,
              static_cast<int>(hdr.gridWidth),
              static_cast<int>(hdr.gridHeight),
              hdr.originX, hdr.originY);
    grid.SetIsometric(true);

    // --- Build occluders from tile LoS data ---
    BuildOccluders(occluders, grid);

    // --- Populate lights ---
    BuildLights(lights);

    // --- Populate roofs ---
    BuildRoofs(roofs);

    // --- Initialize fog to cover the grid ---
    uint32_t fogW = hdr.gridWidth;   // 1:1 with tiles for now
    uint32_t fogH = hdr.gridHeight;
    fog.Init(fogW, fogH,
             grid.GetWorldWidth(), grid.GetWorldHeight(),
             grid.GetOriginX(), grid.GetOriginY());

    // --- Center camera on player spawn (or dev override) ---
    int spawnX = m_scene.playerSpawnX;
    int spawnY = m_scene.playerSpawnY;
    if (m_devModeSpawn)
    {
        spawnX = m_devSpawnX;
        spawnY = m_devSpawnY;
    }
    auto spawnWorld = grid.TileToWorld(spawnX, spawnY);
    camera.SetPosition(spawnWorld.x, spawnWorld.y);

    m_loaded = true;
    return true;
}

void SceneLoader::UnloadScene()
{
    m_scene = SceneData();
    m_loaded = false;
}

// ===========================================================================
// Tile Inspection
// ===========================================================================

TileInspection SceneLoader::InspectTile(int tileX, int tileY) const
{
    TileInspection info;
    info.tileX = tileX;
    info.tileY = tileY;

    if (!m_loaded || !m_scene.InBounds(tileX, tileY))
        return info;

    info.tile = &m_scene.TileAt(tileX, tileY);

    // NPCs
    for (const auto& npc : m_scene.npcs)
    {
        if (npc.character.tileX == tileX && npc.character.tileY == tileY)
            info.npcs.push_back(&npc);
    }

    // Ground items
    for (const auto& gi : m_scene.groundItems)
    {
        if (gi.tileX == tileX && gi.tileY == tileY)
            info.groundItems.push_back(&gi);
    }

    // Quest items
    for (const auto& qi : m_scene.questItems)
    {
        if (qi.tileX == tileX && qi.tileY == tileY)
            info.questItems.push_back(&qi);
    }

    // Triggers
    for (const auto& trg : m_scene.triggers)
    {
        if (TileInBox(tileX, tileY, trg.x0, trg.y0, trg.x1, trg.y1))
            info.triggers.push_back(&trg);
    }

    // Transitions
    for (const auto& trans : m_scene.transitions)
    {
        if (TileInBox(tileX, tileY, trans.x0, trans.y0, trans.x1, trans.y1))
            info.transitions.push_back(&trans);
    }

    // Shop
    for (const auto& shop : m_scene.shops)
    {
        if (shop.tileX == tileX && shop.tileY == tileY)
        {
            info.shop = &shop;
            break;
        }
    }

    // Safehouse
    for (const auto& sh : m_scene.safehouses)
    {
        if (sh.tileX == tileX && sh.tileY == tileY)
        {
            info.safehouse = &sh;
            break;
        }
    }

    // Territory
    for (const auto& ter : m_scene.territories)
    {
        if (TileInBox(tileX, tileY, ter.x0, ter.y0, ter.x1, ter.y1))
        {
            info.territory = &ter;
            break;
        }
    }

    // Player spawn
    info.isPlayerSpawn = (tileX == m_scene.playerSpawnX &&
                          tileY == m_scene.playerSpawnY);

    return info;
}

const SceneNPC* SceneLoader::GetNPCAt(int tileX, int tileY) const
{
    for (const auto& npc : m_scene.npcs)
    {
        if (npc.character.tileX == tileX && npc.character.tileY == tileY)
            return &npc;
    }
    return nullptr;
}

std::vector<const SceneNPC*> SceneLoader::GetAllNPCsAt(int tileX, int tileY) const
{
    std::vector<const SceneNPC*> result;
    for (const auto& npc : m_scene.npcs)
    {
        if (npc.character.tileX == tileX && npc.character.tileY == tileY)
            result.push_back(&npc);
    }
    return result;
}

const SceneGroundItem* SceneLoader::GetGroundItemAt(int tileX, int tileY) const
{
    for (const auto& gi : m_scene.groundItems)
    {
        if (gi.tileX == tileX && gi.tileY == tileY)
            return &gi;
    }
    return nullptr;
}

const SceneTransition* SceneLoader::GetTransitionAt(int tileX, int tileY) const
{
    for (const auto& trans : m_scene.transitions)
    {
        if (TileInBox(tileX, tileY, trans.x0, trans.y0, trans.x1, trans.y1))
            return &trans;
    }
    return nullptr;
}

const SceneTrigger* SceneLoader::GetTriggerAt(int tileX, int tileY) const
{
    for (const auto& trg : m_scene.triggers)
    {
        if (TileInBox(tileX, tileY, trg.x0, trg.y0, trg.x1, trg.y1))
            return &trg;
    }
    return nullptr;
}

const SceneShop* SceneLoader::GetShopAt(int tileX, int tileY) const
{
    for (const auto& shop : m_scene.shops)
    {
        if (shop.tileX == tileX && shop.tileY == tileY)
            return &shop;
    }
    return nullptr;
}

const Safehouse* SceneLoader::GetSafehouseAt(int tileX, int tileY) const
{
    for (const auto& sh : m_scene.safehouses)
    {
        if (sh.tileX == tileX && sh.tileY == tileY)
            return &sh;
    }
    return nullptr;
}

const Territory* SceneLoader::GetTerritoryAt(int tileX, int tileY) const
{
    for (const auto& ter : m_scene.territories)
    {
        if (TileInBox(tileX, tileY, ter.x0, ter.y0, ter.x1, ter.y1))
            return &ter;
    }
    return nullptr;
}

// ===========================================================================
// Populate GameWorld
// ===========================================================================

void SceneLoader::PopulateGameWorld(GameWorld& world) const
{
    if (!m_loaded) return;

    const auto& hdr = m_scene.header;

    // Map tiles
    world.map.Init(static_cast<int>(hdr.gridWidth), static_cast<int>(hdr.gridHeight));
    world.map.tiles = m_scene.tiles;

    // Characters (from NPCs)
    world.characters.clear();
    world.characters.reserve(m_scene.npcs.size());
    for (const auto& npc : m_scene.npcs)
        world.characters.push_back(npc.character);

    // Territories
    world.territories = m_scene.territories;

    // Global state
    world.globalHeat = m_scene.globalHeat;
    world.dayCount   = m_scene.dayCount;

    // Clans (loaded separately via InitClans)
    world.InitClans();
}

// ===========================================================================
// Build helpers
// ===========================================================================

std::vector<bool> SceneLoader::BuildLoSBlockMap() const
{
    uint32_t w = m_scene.header.gridWidth;
    uint32_t h = m_scene.header.gridHeight;
    std::vector<bool> map(w * h, false);
    for (uint32_t i = 0; i < w * h; ++i)
        map[i] = m_scene.tiles[i].blocksLoS;
    return map;
}

std::vector<bool> SceneLoader::BuildWalkMap() const
{
    uint32_t w = m_scene.header.gridWidth;
    uint32_t h = m_scene.header.gridHeight;
    std::vector<bool> map(w * h, true);
    for (uint32_t i = 0; i < w * h; ++i)
        map[i] = !m_scene.tiles[i].blocksMove;
    return map;
}

void SceneLoader::BuildOccluders(engine::OccluderSet& occluders, const engine::Grid& grid) const
{
    occluders.Clear();

    uint32_t w = m_scene.header.gridWidth;
    uint32_t h = m_scene.header.gridHeight;
    uint32_t count = w * h;

    // std::vector<bool> is packed; OccluderSet needs a bool* array, so use
    // a plain array of bool instead.
    std::vector<char> losMap(count, 0);
    for (uint32_t i = 0; i < count; ++i)
        losMap[i] = m_scene.tiles[i].blocksLoS ? 1 : 0;

    if (grid.IsIsometric())
    {
        occluders.BuildFromTileGridIsometric(reinterpret_cast<const bool*>(losMap.data()),
                                              static_cast<int>(w),
                                              static_cast<int>(h),
                                              grid);
    }
    else
    {
        occluders.BuildFromTileGrid(reinterpret_cast<const bool*>(losMap.data()),
                                    static_cast<int>(w),
                                    static_cast<int>(h),
                                    m_scene.header.tileSize,
                                    m_scene.header.originX,
                                    m_scene.header.originY);
    }
}

void SceneLoader::BuildLights(engine::LightSystem& lights) const
{
    lights.Clear();
    for (const auto& sl : m_scene.lights)
    {
        lights.AddLight(sl.worldX, sl.worldY,
                         sl.r, sl.g, sl.b,
                         sl.radius, sl.intensity, sl.flickerPhase);
    }
}

void SceneLoader::BuildRoofs(engine::RoofSystem& roofs) const
{
    roofs.Clear();
    for (const auto& sr : m_scene.roofs)
    {
        engine::TextureHandle tex;
        tex.index = sr.textureId;
        roofs.AddRoof(sr.x0, sr.y0, sr.x1, sr.y1, tex);
    }
}

bool SceneLoader::TileInBox(int tx, int ty, int x0, int y0, int x1, int y1)
{
    return tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1;
}

} // namespace vamp
