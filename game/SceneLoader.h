#pragma once
// SceneLoader.h - Loads a .vmp scene into the engine and game world
//
// Responsibilities:
//   - Parse SceneData from a .vmp file
//   - Populate engine systems (Grid, OccluderSet, LightSystem, FogRenderer,
//     RoofSystem, BackgroundPager, RenderQueue)
//   - Populate game world (GameWorld, characters, items)
//   - Provide tile inspection API (what is on a given tile)

#include "SceneData.h"
#include "SceneFile.h"
#include "GameWorld.h"
#include <string>
#include <vector>
#include <cstdint>

// Engine forward declarations
namespace engine
{
    class Grid;
    class OccluderSet;
    class LightSystem;
    class FogRenderer;
    class RoofSystem;
    class Camera2D;
}

namespace vamp
{

// ---------------------------------------------------------------------------
// TileInspection - everything that exists at a specific tile coordinate
// ---------------------------------------------------------------------------
struct TileInspection
{
    // The tile itself
    const MapTile*              tile            = nullptr;
    int                         tileX           = 0;
    int                         tileY           = 0;

    // Characters standing on this tile
    std::vector<const SceneNPC*> npcs;

    // Items on the ground
    std::vector<const SceneGroundItem*> groundItems;
    std::vector<const SceneQuestItem*>  questItems;

    // Zones that overlap this tile
    std::vector<const SceneTrigger*>    triggers;
    std::vector<const SceneTransition*> transitions;

    // Locations at this tile
    const SceneShop*            shop            = nullptr;
    const Safehouse*            safehouse       = nullptr;

    // Territory this tile belongs to
    const Territory*            territory       = nullptr;

    // Is this the player spawn?
    bool                        isPlayerSpawn   = false;
};

// ---------------------------------------------------------------------------
// SceneLoader - loads/unloads scenes, bridges game data to engine systems
// ---------------------------------------------------------------------------
class SceneLoader
{
public:
    // Load a .vmp file and populate engine + game state.
    // Returns true on success.
    bool LoadScene(const std::string& filePath,
                   engine::Grid& grid,
                   engine::OccluderSet& occluders,
                   engine::LightSystem& lights,
                   engine::FogRenderer& fog,
                   engine::RoofSystem& roofs,
                   engine::Camera2D& camera);

    // Unload the current scene (clear all data)
    void UnloadScene();

    // Is a scene currently loaded?
    bool IsLoaded() const { return m_loaded; }

    // ----- Access to loaded scene data -----

    const SceneData& GetSceneData() const { return m_scene; }
    SceneData&       GetSceneData()       { return m_scene; }
    const SceneHeader& GetHeader() const  { return m_scene.header; }

    // ----- Tile inspection -----

    // Get complete information about what is at a tile coordinate.
    TileInspection InspectTile(int tileX, int tileY) const;

    // Quick queries
    const SceneNPC*         GetNPCAt(int tileX, int tileY) const;
    std::vector<const SceneNPC*> GetAllNPCsAt(int tileX, int tileY) const;
    const SceneGroundItem*  GetGroundItemAt(int tileX, int tileY) const;
    const SceneTransition*  GetTransitionAt(int tileX, int tileY) const;
    const SceneTrigger*     GetTriggerAt(int tileX, int tileY) const;
    const SceneShop*        GetShopAt(int tileX, int tileY) const;
    const Safehouse*        GetSafehouseAt(int tileX, int tileY) const;
    const Territory*        GetTerritoryAt(int tileX, int tileY) const;

    // ----- Populate a GameWorld from loaded scene -----

    void PopulateGameWorld(GameWorld& world) const;

    // ----- Build LoS/walkability arrays from tiles -----

    // Returns a bool array (gridWidth * gridHeight) where true = blocks LoS.
    // Caller is responsible for delete[].
    std::vector<bool> BuildLoSBlockMap() const;

    // Returns a bool array where true = tile is walkable.
    std::vector<bool> BuildWalkMap() const;

private:
    SceneData   m_scene;
    bool        m_loaded = false;

    // Populate engine occluders from tile grid
    void BuildOccluders(engine::OccluderSet& occluders) const;

    // Populate engine lights from scene data
    void BuildLights(engine::LightSystem& lights) const;

    // Populate engine roofs from scene data
    void BuildRoofs(engine::RoofSystem& roofs) const;

    // Helper: check if a tile is inside a bounding box
    static bool TileInBox(int tx, int ty, int x0, int y0, int x1, int y1);
};

} // namespace vamp
