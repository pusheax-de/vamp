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
    EditorContextMenuPage   contextMenuPage = EditorContextMenuPage::Root;
    float                   contextMenuX = 0.0f;
    float                   contextMenuY = 0.0f;
    bool                    dirty           = false;    // Unsaved changes
    std::string             scenePath;                  // Path for Ctrl+S

    // UI overlay (owned by UISystem, raw pointers for update)
    ui::UIPanel*            uiPanel         = nullptr;
    ui::UILabel*            uiHoverLabel    = nullptr;
    ui::UILabel*            uiTileInfoLabel = nullptr;
    ui::UILabel*            uiHintsLabel    = nullptr;

    // Dropdown selectors (owned by UISystem)
    ui::UIDropdown*         terrainDropdown = nullptr;
    ui::UIDropdown*         itemDropdown    = nullptr;

    // Terrain textures (loaded on demand, keyed by TerrainType)
    engine::Texture2D   terrainTextures[static_cast<int>(vamp::TerrainType::COUNT)];
    bool                terrainTexturesLoaded[static_cast<int>(vamp::TerrainType::COUNT)] = {};

    // Object textures (loaded on demand, keyed by SceneObjectType)
    engine::Texture2D   objectTextures[static_cast<int>(vamp::SceneObjectType::COUNT)];
    bool                objectTexturesLoaded[static_cast<int>(vamp::SceneObjectType::COUNT)] = {};

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

        for (int i = 0; i < static_cast<int>(vamp::SceneObjectType::COUNT); ++i)
        {
            if (objectTexturesLoaded[i])
            {
                objectTextures[i].Shutdown(srvHeap);
                objectTexturesLoaded[i] = false;
            }
        }
    }

    void ClearSelection()
    {
        selection = EditorSelection::None;
        selectedTiles.clear();
        selectedPlacedOrdinal = -1;
        selectedObjectIndex = -1;
        selectedGroundItemOrdinal = -1;
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
