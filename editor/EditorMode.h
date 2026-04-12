#pragma once
// EditorMode.h - In-game tile/item editor for .vmp scenes
//
// Activated via --editor command line flag.
// LMB selects a tile for terrain painting, RMB selects a tile for item editing.
// Shift+LMB adds to selection. Hotkeys apply the edit. Esc cancels. Ctrl+S saves.

#include "../engine/Engine.h"
#include "../engine/Texture2D.h"
#include "../game/SceneData.h"
#include "../game/SceneFile.h"
#include "../game/SceneLoader.h"
#include "../ui/UI.h"
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Editor selection mode
// ---------------------------------------------------------------------------
enum class EditorSelection
{
    None,
    Tile,
};

enum class EditorContextMenuPage
{
    Root,
    Ground,
    PlaceItem,
    PlaceObject,
    SelectedItem,
    SelectedObject,
    SelectedLight,
};

// ---------------------------------------------------------------------------
// Tile coordinate pair
// ---------------------------------------------------------------------------
struct TileCoord
{
    int x = -1;
    int y = -1;
    bool operator==(const TileCoord& o) const { return x == o.x && y == o.y; }
};

struct EditorShortcutBinding
{
    uint64_t actionId = 0;
    int      vk = 0;
};

struct EditorTerrainAsset
{
    uint64_t          actionId = 0;
    vamp::TerrainType terrain = vamp::TerrainType::Floor;
    std::string       name;
    std::string       imagePath;
};

struct EditorObjectAsset
{
    uint64_t    typeId = 0;
    std::string name;
    std::string imagePath;
};

struct EditorObjectTextureCacheEntry
{
    std::string       imagePath;
    engine::Texture2D texture;
    bool              loaded = false;
};

// ---------------------------------------------------------------------------
// EditorState - all mutable editor state
// ---------------------------------------------------------------------------
struct EditorState
{
    bool                    active          = false;
    EditorSelection         selection       = EditorSelection::None;
    std::vector<TileCoord>  selectedTiles;              // All selected tiles
    int                     selectedPlacedOrdinal = -1; // Combined placed-content selection for the primary tile
    int                     selectedObjectIndex = -1;   // Scene object targeted by item selection
    int                     selectedGroundItemOrdinal = -1;
    int                     selectedLightIndex = -1;
    EditorContextMenuPage   contextMenuPage = EditorContextMenuPage::Root;
    float                   contextMenuX = 0.0f;
    float                   contextMenuY = 0.0f;
    bool                    dirty           = false;    // Unsaved changes
    std::string             scenePath;                  // Path for Ctrl+S
    std::string             shortcutConfigPath;         // Path for editor.ini

    // UI overlay (owned by UISystem, raw pointers for update)
    ui::UIPanel*            uiPanel         = nullptr;
    ui::UILabel*            uiHoverLabel    = nullptr;
    ui::UILabel*            uiTileInfoLabel = nullptr;
    ui::UILabel*            uiHintsLabel    = nullptr;

    // Dropdown selectors (owned by UISystem)
    ui::UIDropdown*         terrainDropdown = nullptr;
    ui::UIDropdown*         itemDropdown    = nullptr;
    std::vector<EditorShortcutBinding> shortcutBindings;
    std::vector<EditorTerrainAsset> terrainAssets;
    std::vector<EditorObjectAsset>  objectAssets;
    std::unordered_map<uint64_t, size_t> terrainAssetIndexByActionId;
    std::unordered_map<uint64_t, size_t> objectAssetIndexByTypeId;

    // Terrain textures (loaded on demand, keyed by TerrainType)
    engine::Texture2D   terrainTextures[static_cast<int>(vamp::TerrainType::COUNT)];
    bool                terrainTexturesLoaded[static_cast<int>(vamp::TerrainType::COUNT)] = {};
    std::string         terrainTexturePaths[static_cast<int>(vamp::TerrainType::COUNT)];

    // Object textures (loaded on demand, keyed by image path)
    std::vector<EditorObjectTextureCacheEntry> objectTextureCache;
    engine::Texture2D      lightMarkerTexture;
    bool                   lightMarkerTextureLoaded = false;

    void ShutdownTextures(engine::PersistentDescriptorAllocator& srvHeap)
    {
        for (int i = 0; i < static_cast<int>(vamp::TerrainType::COUNT); ++i)
        {
            if (terrainTexturesLoaded[i])
            {
                terrainTextures[i].Shutdown(srvHeap);
                terrainTexturesLoaded[i] = false;
            }
        }

        for (auto& entry : objectTextureCache)
        {
            if (entry.loaded)
            {
                entry.texture.Shutdown(srvHeap);
                entry.loaded = false;
            }
        }
        objectTextureCache.clear();

        if (lightMarkerTextureLoaded)
        {
            lightMarkerTexture.Shutdown(srvHeap);
            lightMarkerTextureLoaded = false;
        }
    }

    void ClearSelection()
    {
        selection = EditorSelection::None;
        selectedTiles.clear();
        selectedPlacedOrdinal = -1;
        selectedObjectIndex = -1;
        selectedGroundItemOrdinal = -1;
        selectedLightIndex = -1;
    }

    bool HasTile(int tx, int ty) const
    {
        TileCoord tc{ tx, ty };
        return std::find(selectedTiles.begin(), selectedTiles.end(), tc) != selectedTiles.end();
    }

    void AddTile(int tx, int ty)
    {
        TileCoord tc{ tx, ty };
        if (!HasTile(tx, ty))
            selectedTiles.push_back(tc);
    }

    void SetSingleTile(int tx, int ty)
    {
        selectedTiles.clear();
        selectedTiles.push_back({ tx, ty });
    }
};

// ---------------------------------------------------------------------------
// Editor public API (called from vampire.cpp)
// ---------------------------------------------------------------------------

// Create editor UI elements. Call once after UISystem::Init().
void EditorInitUI(ui::UISystem& uiSystem, EditorState& editor);

// Process one editor frame (input + render). Called instead of RenderFrame().
void EditorFrame(engine::RendererD3D12& renderer,
                 engine::SceneRenderer& sceneRenderer,
                 engine::Camera2D& camera,
                 engine::Grid& grid,
                 engine::BackgroundPager& bgPager,
                 engine::RenderQueue& renderQueue,
                 engine::LightSystem& lights,
                 engine::OccluderSet& occluders,
                 engine::FogRenderer& fog,
                 engine::RoofSystem& roofs,
                 engine::InputSystem& input,
                 vamp::SceneLoader& sceneLoader,
                 EditorState& editor,
                 ui::UISystem& uiSystem,
                 HWND hWnd,
                 float& time,
                 LARGE_INTEGER& timerFreq,
                 LARGE_INTEGER& timerLast);
